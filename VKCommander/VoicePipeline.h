#pragma once

#include "SileroVAD.h"
#include <vector>
#include <string>
#include <mutex>
#include <functional>

struct whisper_context;

class FVoicePipeline
{
public:
    static constexpr float VAD_THRESHOLD = 0.5f;
    static constexpr float RMS_THRESHOLD = 0.015f;
    static constexpr int32_t SILENCE_CHUNKS_LIMIT = 25;

    using FOnSpeechRecognized = std::function<void(std::string const&)>;

    FVoicePipeline(wchar_t const* VadModelPath = L"VAD/silero_vad.onnx", char const* WhisperModelPath = "ggml-base.bin");
    ~FVoicePipeline();

    void ProcessAudioChunk(float const* ChunkData);
    void SetOnSpeechRecognized(FOnSpeechRecognized Callback);

protected:
    void ExecuteSTT(std::vector<float> const& AudioData);

    FSileroVAD mVAD;
    whisper_context* mWhisperContext;
    bool mIsSpeaking;
    int32_t mSilenceChunkCount;
    std::vector<float> mSpeechBuffer;
    std::mutex mBufferMutex;
    std::mutex mWhisperMutex;
    FOnSpeechRecognized mOnSpeechRecognized;
};
