#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <iostream>
#include <vector>
#include <string>
#include <mutex>
#include <thread>
#include <cmath>
#include <onnxruntime_cxx_api.h>
#include "whisper.h"

constexpr uint32_t SAMPLE_RATE = 16000;
constexpr size_t VAD_CHUNK_SIZE = 512;
constexpr float VAD_THRESHOLD = 0.5f;
constexpr int32_t SILENCE_CHUNKS_LIMIT = 25;

constexpr float RMS_THRESHOLD = 0.015f;

class FSileroVAD
{
public:
    FSileroVAD(wchar_t const* ModelPath)
        : mEnv(ORT_LOGGING_LEVEL_WARNING, "SileroVAD")
    {
        try
        {
            Ort::SessionOptions SessionOptions;
            SessionOptions.SetIntraOpNumThreads(1);
            mSession = std::make_unique<Ort::Session>(mEnv, ModelPath, SessionOptions);
            mState.assign(2 * 1 * 128, 0.0f);
        }
        catch (Ort::Exception const& Exception)
        {
            std::cerr << "VAD 초기화 실패: " << Exception.what() << std::endl;
        }
    }

    float Predict(float const* ChunkData)
    {
        if (ChunkData == nullptr || !mSession)
        {
            return 0.0f;
        }

        Ort::MemoryInfo MemoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

        std::vector<int64_t> InputShape = { 1, static_cast<int64_t>(VAD_CHUNK_SIZE) };
        Ort::Value InputTensor = Ort::Value::CreateTensor<float>(
            MemoryInfo, const_cast<float*>(ChunkData), VAD_CHUNK_SIZE, InputShape.data(), InputShape.size());

        std::vector<int64_t> StateShape = { 2, 1, 128 };
        Ort::Value StateTensor = Ort::Value::CreateTensor<float>(
            MemoryInfo, mState.data(), mState.size(), StateShape.data(), StateShape.size());

        int64_t SrValue = SAMPLE_RATE;
        std::vector<int64_t> SrShape = { 1 };
        Ort::Value SrTensor = Ort::Value::CreateTensor<int64_t>(
            MemoryInfo, &SrValue, 1, SrShape.data(), SrShape.size());

        char const* InputNames[] = { "input", "state", "sr" };
        char const* OutputNames[] = { "output", "stateN" };

        Ort::Value Inputs[] = { std::move(InputTensor), std::move(StateTensor), std::move(SrTensor) };

        try
        {
            auto Outputs = mSession->Run(
                Ort::RunOptions{ nullptr },
                InputNames, Inputs, 3,
                OutputNames, 2);

            float Probability = 0.0f;
            float const* NextState = nullptr;

            for (size_t i = 0; i < Outputs.size(); ++i)
            {
                size_t const ElementCount = Outputs[i].GetTensorTypeAndShapeInfo().GetElementCount();
                if (ElementCount == 1)
                {
                    Probability = Outputs[i].GetTensorMutableData<float>()[0];
                }
                else if (ElementCount == mState.size())
                {
                    NextState = Outputs[i].GetTensorMutableData<float>();
                }
            }

            if (NextState != nullptr)
            {
                std::copy(NextState, NextState + mState.size(), mState.begin());
            }

            return Probability;
        }
        catch (Ort::Exception const&)
        {
            return 0.0f;
        }
    }

    void ResetState()
    {
        std::fill(mState.begin(), mState.end(), 0.0f);
    }

protected:
    Ort::Env mEnv;
    std::unique_ptr<Ort::Session> mSession;
    std::vector<float> mState;
};

class FVoicePipeline
{
public:
    FVoicePipeline()
        : mVAD(L"VAD/silero_vad.onnx")
        , mWhisperContext(nullptr)
        , mIsSpeaking(false)
        , mSilenceChunkCount(0)
    {
        whisper_context_params ContextParams = whisper_context_default_params();
        mWhisperContext = whisper_init_from_file_with_params("ggml-base.bin", ContextParams);
        if (!mWhisperContext)
        {
            std::cerr << "Whisper 모델 초기화 실패" << std::endl;
        }
    }

    ~FVoicePipeline()
    {
        if (mWhisperContext)
        {
            whisper_free(mWhisperContext);
            mWhisperContext = nullptr;
        }
    }

    void ProcessAudioChunk(float const* ChunkData)
    {
        if (ChunkData == nullptr)
        {
            return;
        }

        float SumSquare = 0.0f;
        for (size_t i = 0; i < VAD_CHUNK_SIZE; ++i)
        {
            SumSquare += ChunkData[i] * ChunkData[i];
        }
        float const Rms = std::sqrt(SumSquare / static_cast<float>(VAD_CHUNK_SIZE));
        float const Probability = mVAD.Predict(ChunkData);

        bool const bIsSpeech = (Probability >= VAD_THRESHOLD) || (Rms >= RMS_THRESHOLD);

        if (bIsSpeech)
        {
            if (!mIsSpeaking)
            {
                mIsSpeaking = true;
                std::cout << "\n[음성 감지 시작] 말하는 중..." << std::endl;
            }
            mSilenceChunkCount = 0;

            std::lock_guard<std::mutex> Lock(mBufferMutex);
            mSpeechBuffer.insert(mSpeechBuffer.end(), ChunkData, ChunkData + VAD_CHUNK_SIZE);
            return;
        }

        if (!mIsSpeaking)
        {
            return;
        }

        mSilenceChunkCount++;
        {
            std::lock_guard<std::mutex> Lock(mBufferMutex);
            mSpeechBuffer.insert(mSpeechBuffer.end(), ChunkData, ChunkData + VAD_CHUNK_SIZE);
        }

        if (mSilenceChunkCount >= SILENCE_CHUNKS_LIMIT)
        {
            mIsSpeaking = false;
            mSilenceChunkCount = 0;
            std::cout << "[음성 종료 감지] 디코딩 시작..." << std::endl;

            std::vector<float> AudioToProcess;
            {
                std::lock_guard<std::mutex> Lock(mBufferMutex);
                AudioToProcess = std::move(mSpeechBuffer);
                mSpeechBuffer.clear();
            }

            mVAD.ResetState();
            std::thread([this, Data = std::move(AudioToProcess)]()
            {
                ExecuteSTT(Data);
            }).detach();
        }
    }

protected:
    void ExecuteSTT(std::vector<float> const& AudioData)
    {
        if (!mWhisperContext)
        {
            return;
        }

        if (AudioData.size() < SAMPLE_RATE * 0.5f)
        {
            return;
        }

        std::lock_guard<std::mutex> WhisperLock(mWhisperMutex);

        whisper_full_params Params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
        Params.language = "ko";
        Params.n_threads = 4;
        Params.no_context = true;
        Params.single_segment = true;
        Params.print_progress = false;
        Params.print_realtime = false;
        Params.print_timestamps = false;

        if (whisper_full(mWhisperContext, Params, AudioData.data(), static_cast<int>(AudioData.size())) != 0)
        {
            std::cerr << "STT 디코딩 실패" << std::endl;
            return;
        }

        int const SegmentCount = whisper_full_n_segments(mWhisperContext);
        std::string ResultText = "";
        for (int i = 0; i < SegmentCount; ++i)
        {
            ResultText += whisper_full_get_segment_text(mWhisperContext, i);
        }

        std::cout << "\n\n=============================================" << std::endl;
        std::cout << ">> [Whisper STT 인식 결과]: " << ResultText << std::endl;
        std::cout << "=============================================\n" << std::endl;
    }

    FSileroVAD mVAD;
    whisper_context* mWhisperContext;
    bool mIsSpeaking;
    int32_t mSilenceChunkCount;
    std::vector<float> mSpeechBuffer;
    std::mutex mBufferMutex;
    std::mutex mWhisperMutex;
};

FVoicePipeline* gPipeline = nullptr;
std::vector<float> gAccumulatedInput;

void AudioDataCallback(ma_device* pDevice, void* pOutput, void const* pInput, ma_uint32 FrameCount)
{
    if (!pInput || !gPipeline)
    {
        return;
    }

    float const* FloatInput = static_cast<float const*>(pInput);
    gAccumulatedInput.insert(gAccumulatedInput.end(), FloatInput, FloatInput + FrameCount);

    while (gAccumulatedInput.size() >= VAD_CHUNK_SIZE)
    {
        gPipeline->ProcessAudioChunk(gAccumulatedInput.data());
        gAccumulatedInput.erase(gAccumulatedInput.begin(), gAccumulatedInput.begin() + VAD_CHUNK_SIZE);
    }
}

int main()
{
    gPipeline = new FVoicePipeline();

    ma_context Context;
    if (ma_context_init(nullptr, 0, nullptr, &Context) != MA_SUCCESS)
    {
        std::cerr << "오디오 컨텍스트 초기화 실패" << std::endl;
        delete gPipeline;
        return -1;
    }

    ma_device_info* pCaptureInfos = nullptr;
    ma_uint32 CaptureCount = 0;
    if (ma_context_get_devices(&Context, nullptr, nullptr, &pCaptureInfos, &CaptureCount) != MA_SUCCESS || CaptureCount == 0)
    {
        std::cerr << "연결된 마이크 장치를 찾을 수 없습니다." << std::endl;
        ma_context_uninit(&Context);
        delete gPipeline;
        return -1;
    }

    std::cout << "\n=== 사용 가능한 마이크 장치 목록 (" << CaptureCount << "개) ===" << std::endl;
    ma_uint32 SelectedIndex = 0;
    for (ma_uint32 i = 0; i < CaptureCount; ++i)
    {
        std::cout << " [" << i << "] " << pCaptureInfos[i].name;
        if (pCaptureInfos[i].isDefault)
        {
            std::cout << " (기본 장치)";
            SelectedIndex = i;
        }
        std::cout << std::endl;
    }

    if (CaptureCount > 1)
    {
        std::cout << "사용할 마이크 번호를 입력하세요 (기본 장치 사용: 엔터): ";
        std::string InputLine;
        std::getline(std::cin, InputLine);
        if (!InputLine.empty())
        {
            try
            {
                int const InputIndex = std::stoi(InputLine);
                if (InputIndex >= 0 && InputIndex < static_cast<int>(CaptureCount))
                {
                    SelectedIndex = static_cast<ma_uint32>(InputIndex);
                }
            }
            catch (...)
            {
            }
        }
    }

    std::cout << "선택된 장치: [" << SelectedIndex << "] " << pCaptureInfos[SelectedIndex].name << std::endl;

    ma_device_config DeviceConfig = ma_device_config_init(ma_device_type_capture);
    DeviceConfig.capture.pDeviceID = &pCaptureInfos[SelectedIndex].id;
    DeviceConfig.capture.format = ma_format_f32;
    DeviceConfig.capture.channels = 1;
    DeviceConfig.sampleRate = SAMPLE_RATE;
    DeviceConfig.dataCallback = AudioDataCallback;

    ma_device Device;
    if (ma_device_init(&Context, &DeviceConfig, &Device) != MA_SUCCESS)
    {
        std::cerr << "마이크 디바이스 초기화 실패" << std::endl;
        ma_context_uninit(&Context);
        delete gPipeline;
        return -1;
    }

    if (ma_device_start(&Device) != MA_SUCCESS)
    {
        std::cerr << "마이크 캡처 시작 실패" << std::endl;
        ma_device_uninit(&Device);
        ma_context_uninit(&Context);
        delete gPipeline;
        return -1;
    }

    std::cout << "[오디오 장치 정보] 샘플레이트: " << Device.sampleRate << "Hz, 채널: " << Device.capture.channels << std::endl;

    std::cout << "\n=== 마이크 음성 인식 준비 완료 ===" << std::endl;
    std::cout << "마이크에 대고 말을 시작하세요 (종료하려면 엔터 키 입력)..." << std::endl;

    std::cin.get();

    ma_device_uninit(&Device);
    ma_context_uninit(&Context);
    delete gPipeline;
    return 0;
}
