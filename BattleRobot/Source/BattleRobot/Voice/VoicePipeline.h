#pragma once

#include "CoreMinimal.h"
#include "SileroVAD.h"
#include <vector>
#include <memory>
#include <mutex>

struct whisper_context;
struct ma_context;
struct ma_device;

class BATTLEROBOT_API FVoicePipeline
{
public:
    static constexpr float VAD_THRESHOLD = 0.5f;
    static constexpr float RMS_THRESHOLD = 0.015f;
    static constexpr int32_t SILENCE_CHUNKS_LIMIT = 25;

    using FOnSpeechRecognized = TFunction<void(FString const&)>;

    FVoicePipeline(FString const& VadModelPath, FString const& WhisperModelPath);
    ~FVoicePipeline();

    bool StartCapture();
    void StopCapture();

    void ProcessAudioChunk(float const* ChunkData);
    void SetOnSpeechRecognized(FOnSpeechRecognized Callback);

protected:
    void ExecuteSTT(std::vector<float> const& AudioData);
    void OnAudioInput(float const* InputData, size_t FrameCount);

    FSileroVAD mVAD;
    whisper_context* mWhisperContext;
    bool mIsSpeaking;
    int32_t mSilenceChunkCount;
    std::vector<float> mSpeechBuffer;
    std::vector<float> mAccumulatedInput;
    std::mutex mBufferMutex;
    std::mutex mWhisperMutex;
    FOnSpeechRecognized mOnSpeechRecognized;

    std::unique_ptr<ma_context> mAudioContext;
    std::unique_ptr<ma_device> mAudioDevice;
    bool mIsCapturing;
};
