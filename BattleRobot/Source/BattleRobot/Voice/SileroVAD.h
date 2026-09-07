#pragma once

#include "CoreMinimal.h"
#include <vector>
#include <memory>
#include <cstdint>
#include <onnxruntime_cxx_api.h>

class BATTLEROBOT_API FSileroVAD
{
public:
    static constexpr uint32_t SAMPLE_RATE = 16000;
    static constexpr size_t VAD_CHUNK_SIZE = 512;

    FSileroVAD(FString const& ModelPath);
    ~FSileroVAD() = default;

    float Predict(float const* ChunkData);
    void ResetState();

protected:
    std::unique_ptr<Ort::Env> mEnv;
    std::unique_ptr<Ort::Session> mSession;
    std::vector<float> mState;
};
