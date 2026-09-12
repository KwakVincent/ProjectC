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
    static constexpr int32_t DEFAULT_SILENCE_CHUNKS_LIMIT = 3;

    using FOnSpeechRecognized = TFunction<void(FString const&, double VadDurationMs, double SttDurationMs)>;

    FVoicePipeline(FString const& VadModelPath, FString const& WhisperModelPath, int32_t InitialSilenceLimit = DEFAULT_SILENCE_CHUNKS_LIMIT, float InitialBufferTimeoutSec = 0.5f);
    ~FVoicePipeline();

    bool StartCapture();
    void StopCapture();
    bool IsCapturing() const;

    void ProcessAudioChunk(float const* ChunkData);
    void SetOnSpeechRecognized(FOnSpeechRecognized Callback);

    void SetSilenceLimit(int32_t NewLimit);
    int32_t GetSilenceLimit() const;
    void SetBufferRetentionTimeout(float NewTimeoutSec);
    float GetBufferRetentionTimeout() const;
    void OnActionEvaluationFeedback(int32_t ValidActionCount);

protected:
    void ExecuteSTT(std::vector<float> const& AudioData, double VadDurationMs);
    void OnAudioInput(float const* InputData, size_t FrameCount);

    FSileroVAD mVAD;
    whisper_context* mWhisperContext;
    bool mIsSpeaking;
    int32_t mSilenceChunkCount;
    int32_t mSilenceChunksLimit;
    double mSpeechStartTime;
    double mLastSpeechEndTime;
    float mBufferRetentionTimeoutSec;
    bool mbHasPendingFailedBuffer;
    std::vector<float> mSpeechBuffer;
    std::vector<float> mAccumulatedInput;
    std::mutex mBufferMutex;
    std::mutex mWhisperMutex;
    FOnSpeechRecognized mOnSpeechRecognized;

    std::unique_ptr<ma_context> mAudioContext;
    std::unique_ptr<ma_device> mAudioDevice;
    bool mIsCapturing;
};
