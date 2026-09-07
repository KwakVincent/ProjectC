#include "SileroVAD.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include <algorithm>

FSileroVAD::FSileroVAD(FString const& ModelPath)
    : mEnv(nullptr)
    , mSession(nullptr)
{
    FString DllPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/ThirdParty/Onnx/Lib/onnxruntime.dll"));
    if (!FPaths::FileExists(DllPath))
    {
        DllPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Binaries/Win64/onnxruntime.dll"));
    }
    void* DllHandle = FPlatformProcess::GetDllHandle(*DllPath);

    using OrtGetApiBaseFn = const OrtApiBase* (ORT_API_CALL*)(void);
    OrtGetApiBaseFn GetApiBaseFunc = nullptr;
    if (DllHandle != nullptr)
    {
        GetApiBaseFunc = reinterpret_cast<OrtGetApiBaseFn>(FPlatformProcess::GetDllExport(DllHandle, TEXT("OrtGetApiBase")));
    }

    const OrtApiBase* ApiBase = (GetApiBaseFunc != nullptr) ? GetApiBaseFunc() : OrtGetApiBase();
    UE_LOG(LogTemp, Warning, TEXT("[VAD 디버그] ApiBase: %p (직접 바인딩: %s)"), ApiBase, (GetApiBaseFunc != nullptr) ? TEXT("성공") : TEXT("실패"));
    if (ApiBase == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("[VAD 오류] OrtGetApiBase()가 nullptr를 반환했습니다!"));
        return;
    }

    const OrtApi* Api = ApiBase->GetApi(ORT_API_VERSION);
    if (Api == nullptr)
    {
        for (uint32_t v = 24; v >= 1; --v)
        {
            Api = ApiBase->GetApi(v);
            if (Api != nullptr)
            {
                UE_LOG(LogTemp, Warning, TEXT("[VAD 디버그] 하위 호환 API 버전 %d 선택됨"), v);
                break;
            }
        }
    }
    UE_LOG(LogTemp, Warning, TEXT("[VAD 디버그] 최종 Api pointer: %p"), Api);

    if (Api == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("[VAD 오류] 호환 가능한 OrtApi를 찾을 수 없습니다!"));
        return;
    }

    Ort::detail::Global::Api(Api);

    try
    {
        mEnv = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "SileroVAD");
        Ort::SessionOptions SessionOptions;
        SessionOptions.SetIntraOpNumThreads(1);
        mSession = std::make_unique<Ort::Session>(*mEnv, *ModelPath, SessionOptions);
        mState.assign(2 * 1 * 128, 0.0f);
    }
    catch (Ort::Exception const& Exception)
    {
        UE_LOG(LogTemp, Error, TEXT("VAD 초기화 실패: %s"), UTF8_TO_TCHAR(Exception.what()));
    }
    catch (...)
    {
        UE_LOG(LogTemp, Error, TEXT("VAD 초기화 중 알 수 없는 예외 발생"));
    }
}

float FSileroVAD::Predict(float const* ChunkData)
{
    if (ChunkData == nullptr || !mSession || !mEnv)
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
