#include "VoicePipeline.h"

#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "Windows/HideWindowsPlatformTypes.h"

#include "whisper.h"
#include <cmath>
#include <thread>

static void MaDataCallback(ma_device* pDevice, void* pOutput, void const* pInput, ma_uint32 FrameCount)
{
    if (pDevice == nullptr || pInput == nullptr || pDevice->pUserData == nullptr)
    {
        return;
    }

    FVoicePipeline* Pipeline = static_cast<FVoicePipeline*>(pDevice->pUserData);
    Pipeline->ProcessAudioChunk(nullptr);
}

FVoicePipeline::FVoicePipeline(FString const& VadModelPath, FString const& WhisperModelPath)
    : mVAD(VadModelPath)
    , mWhisperContext(nullptr)
    , mIsSpeaking(false)
    , mSilenceChunkCount(0)
    , mOnSpeechRecognized(nullptr)
    , mAudioContext(nullptr)
    , mAudioDevice(nullptr)
    , mIsCapturing(false)
{
    whisper_context_params ContextParams = whisper_context_default_params();
    mWhisperContext = whisper_init_from_file_with_params(TCHAR_TO_UTF8(*WhisperModelPath), ContextParams);
    if (!mWhisperContext)
    {
        UE_LOG(LogTemp, Error, TEXT("Whisper 모델 초기화 실패: %s"), *WhisperModelPath);
    }
}

FVoicePipeline::~FVoicePipeline()
{
    StopCapture();

    if (mWhisperContext)
    {
        whisper_free(mWhisperContext);
        mWhisperContext = nullptr;
    }
}

void FVoicePipeline::OnAudioInput(float const* InputData, size_t FrameCount)
{
    if (InputData == nullptr || FrameCount == 0)
    {
        return;
    }

    mAccumulatedInput.insert(mAccumulatedInput.end(), InputData, InputData + FrameCount);

    while (mAccumulatedInput.size() >= FSileroVAD::VAD_CHUNK_SIZE)
    {
        ProcessAudioChunk(mAccumulatedInput.data());
        mAccumulatedInput.erase(mAccumulatedInput.begin(), mAccumulatedInput.begin() + FSileroVAD::VAD_CHUNK_SIZE);
    }
}

bool FVoicePipeline::StartCapture()
{
    if (mIsCapturing)
    {
        return true;
    }

    mAudioContext = std::make_unique<ma_context>();
    if (ma_context_init(nullptr, 0, nullptr, mAudioContext.get()) != MA_SUCCESS)
    {
        UE_LOG(LogTemp, Error, TEXT("오디오 컨텍스트 초기화 실패"));
        mAudioContext.reset();
        return false;
    }

    ma_device_config DeviceConfig = ma_device_config_init(ma_device_type_capture);
    DeviceConfig.capture.format = ma_format_f32;
    DeviceConfig.capture.channels = 1;
    DeviceConfig.sampleRate = FSileroVAD::SAMPLE_RATE;
    DeviceConfig.pUserData = this;
    DeviceConfig.dataCallback = [](ma_device* pDevice, void* pOutput, void const* pInput, ma_uint32 FrameCount)
    {
        if (pDevice == nullptr || pInput == nullptr || pDevice->pUserData == nullptr)
        {
            return;
        }

        FVoicePipeline* Pipeline = static_cast<FVoicePipeline*>(pDevice->pUserData);
        Pipeline->OnAudioInput(static_cast<float const*>(pInput), static_cast<size_t>(FrameCount));
    };

    mAudioDevice = std::make_unique<ma_device>();
    if (ma_device_init(mAudioContext.get(), &DeviceConfig, mAudioDevice.get()) != MA_SUCCESS)
    {
        UE_LOG(LogTemp, Error, TEXT("마이크 디바이스 초기화 실패"));
        ma_context_uninit(mAudioContext.get());
        mAudioDevice.reset();
        mAudioContext.reset();
        return false;
    }

    if (ma_device_start(mAudioDevice.get()) != MA_SUCCESS)
    {
        UE_LOG(LogTemp, Error, TEXT("마이크 캡처 시작 실패"));
        ma_device_uninit(mAudioDevice.get());
        ma_context_uninit(mAudioContext.get());
        mAudioDevice.reset();
        mAudioContext.reset();
        return false;
    }

    mIsCapturing = true;
    UE_LOG(LogTemp, Log, TEXT("마이크 음성 캡처 시작 완료 (샘플레이트: %dHz)"), mAudioDevice->sampleRate);
    return true;
}

void FVoicePipeline::StopCapture()
{
    if (!mIsCapturing)
    {
        return;
    }

    if (mAudioDevice)
    {
        ma_device_stop(mAudioDevice.get());
        ma_device_uninit(mAudioDevice.get());
        mAudioDevice.reset();
    }

    if (mAudioContext)
    {
        ma_context_uninit(mAudioContext.get());
        mAudioContext.reset();
    }

    mIsCapturing = false;
    UE_LOG(LogTemp, Log, TEXT("마이크 음성 캡처 중지 완료"));
}

void FVoicePipeline::SetOnSpeechRecognized(FOnSpeechRecognized Callback)
{
    mOnSpeechRecognized = MoveTemp(Callback);
}

void FVoicePipeline::ProcessAudioChunk(float const* ChunkData)
{
    if (ChunkData == nullptr)
    {
        return;
    }

    float SumSquare = 0.0f;
    for (size_t i = 0; i < FSileroVAD::VAD_CHUNK_SIZE; ++i)
    {
        SumSquare += ChunkData[i] * ChunkData[i];
    }
    float const Rms = std::sqrt(SumSquare / static_cast<float>(FSileroVAD::VAD_CHUNK_SIZE));
    float const Probability = mVAD.Predict(ChunkData);

    bool const bIsSpeech = (Probability >= VAD_THRESHOLD) || (Rms >= RMS_THRESHOLD);

    if (bIsSpeech)
    {
        if (!mIsSpeaking)
        {
            mIsSpeaking = true;
            UE_LOG(LogTemp, Log, TEXT("[음성 감지 시작] 말하는 중..."));
        }
        mSilenceChunkCount = 0;

        std::lock_guard<std::mutex> Lock(mBufferMutex);
        mSpeechBuffer.insert(mSpeechBuffer.end(), ChunkData, ChunkData + FSileroVAD::VAD_CHUNK_SIZE);
        return;
    }

    if (!mIsSpeaking)
    {
        return;
    }

    mSilenceChunkCount++;
    {
        std::lock_guard<std::mutex> Lock(mBufferMutex);
        mSpeechBuffer.insert(mSpeechBuffer.end(), ChunkData, ChunkData + FSileroVAD::VAD_CHUNK_SIZE);
    }

    if (mSilenceChunkCount >= SILENCE_CHUNKS_LIMIT)
    {
        mIsSpeaking = false;
        mSilenceChunkCount = 0;
        UE_LOG(LogTemp, Log, TEXT("[음성 종료 감지] 디코딩 시작..."));

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

void FVoicePipeline::ExecuteSTT(std::vector<float> const& AudioData)
{
    if (!mWhisperContext)
    {
        return;
    }

    if (AudioData.size() < static_cast<size_t>(FSileroVAD::SAMPLE_RATE * 0.3f))
    {
        return;
    }

    std::lock_guard<std::mutex> WhisperLock(mWhisperMutex);

    whisper_full_params Params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    unsigned int const HardwareThreads = std::thread::hardware_concurrency();
    int const ThreadCount = HardwareThreads > 0 ? static_cast<int>(FMath::Clamp(HardwareThreads, 4u, 8u)) : 4;
    Params.n_threads = ThreadCount;
    Params.language = "ko";
    Params.no_context = true;
    Params.single_segment = true;
    Params.no_timestamps = true;
    Params.max_tokens = 16;
    Params.temperature = 0.0f;
    Params.temperature_inc = 0.0f;
    Params.print_progress = false;
    Params.print_realtime = false;
    Params.print_timestamps = false;

    if (whisper_full(mWhisperContext, Params, AudioData.data(), static_cast<int>(AudioData.size())) != 0)
    {
        UE_LOG(LogTemp, Error, TEXT("STT 디코딩 실패"));
        return;
    }

    int const SegmentCount = whisper_full_n_segments(mWhisperContext);
    FString ResultText;
    for (int i = 0; i < SegmentCount; ++i)
    {
        char const* Segment = whisper_full_get_segment_text(mWhisperContext, i);
        if (Segment != nullptr)
        {
            ResultText += UTF8_TO_TCHAR(Segment);
        }
    }

    ResultText.TrimStartAndEndInline();
    UE_LOG(LogTemp, Log, TEXT(">> [Whisper STT 인식 결과]: %s"), *ResultText);

    if (mOnSpeechRecognized)
    {
        mOnSpeechRecognized(ResultText);
    }
}
