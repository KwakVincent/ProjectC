#include "VoicePipeline.h"
#include "whisper.h"
#include <iostream>
#include <cmath>
#include <thread>

FVoicePipeline::FVoicePipeline(wchar_t const* VadModelPath, char const* WhisperModelPath)
    : mVAD(VadModelPath)
    , mWhisperContext(nullptr)
    , mIsSpeaking(false)
    , mSilenceChunkCount(0)
    , mOnSpeechRecognized(nullptr)
{
    whisper_context_params ContextParams = whisper_context_default_params();
    mWhisperContext = whisper_init_from_file_with_params(WhisperModelPath, ContextParams);
    if (!mWhisperContext)
    {
        std::cerr << "Whisper 모델 초기화 실패" << std::endl;
    }
}

FVoicePipeline::~FVoicePipeline()
{
    if (mWhisperContext)
    {
        whisper_free(mWhisperContext);
        mWhisperContext = nullptr;
    }
}

void FVoicePipeline::SetOnSpeechRecognized(FOnSpeechRecognized Callback)
{
    mOnSpeechRecognized = std::move(Callback);
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
            std::cout << "\n[음성 감지 시작] 말하는 중..." << std::endl;
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

void FVoicePipeline::ExecuteSTT(std::vector<float> const& AudioData)
{
    if (!mWhisperContext)
    {
        return;
    }

    if (AudioData.size() < FSileroVAD::SAMPLE_RATE * 0.5f)
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

    if (mOnSpeechRecognized)
    {
        mOnSpeechRecognized(ResultText);
    }
}
