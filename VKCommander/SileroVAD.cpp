#include "SileroVAD.h"
#include <iostream>
#include <algorithm>

FSileroVAD::FSileroVAD(wchar_t const* ModelPath)
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

float FSileroVAD::Predict(float const* ChunkData)
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

void FSileroVAD::ResetState()
{
    std::fill(mState.begin(), mState.end(), 0.0f);
}
