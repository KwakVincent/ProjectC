#include "QwenOnDeviceRunner.h"

#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include "ort_genai.h"
#include "Windows/HideWindowsPlatformTypes.h"

#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include <mutex>
#include <string>

struct FQwenOnDeviceRunner::FImpl
{
    std::unique_ptr<OgaModel> mModel;
    std::unique_ptr<OgaTokenizer> mTokenizer;
    std::mutex mInferenceMutex;
    void* mOrtDllHandle = nullptr;
    void* mGenAiDllHandle = nullptr;
};

FQwenOnDeviceRunner::FQwenOnDeviceRunner()
    : mImpl(MakeUnique<FImpl>())
    , mbIsInitialized(false)
{
}

FQwenOnDeviceRunner::~FQwenOnDeviceRunner()
{
    Shutdown();
}

bool FQwenOnDeviceRunner::Initialize(FString const& ModelFolderPath)
{
    if (mbIsInitialized)
    {
        return true;
    }

    if (ModelFolderPath.IsEmpty() || !mImpl)
    {
        return false;
    }

    FString const FullModelPath = FPaths::ConvertRelativePathToFull(ModelFolderPath);
    if (!FPaths::DirectoryExists(FullModelPath))
    {
        UE_LOG(LogTemp, Error, TEXT("Qwen2.5 모델 디렉토리를 찾을 수 없습니다: %s"), *FullModelPath);
        return false;
    }

    FString OrtDllPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Binaries/Win64/onnxruntime.dll"));
    if (!FPaths::FileExists(OrtDllPath))
    {
        OrtDllPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/ThirdParty/Onnx/Lib/onnxruntime.dll"));
    }
    mImpl->mOrtDllHandle = FPlatformProcess::GetDllHandle(*OrtDllPath);

    FString GenAiDllPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Binaries/Win64/onnxruntime-genai.dll"));
    if (!FPaths::FileExists(GenAiDllPath))
    {
        GenAiDllPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/ThirdParty/Onnx/Lib/onnxruntime-genai.dll"));
    }
    mImpl->mGenAiDllHandle = FPlatformProcess::GetDllHandle(*GenAiDllPath);
    if (mImpl->mGenAiDllHandle == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("onnxruntime-genai.dll 로드 실패: %s"), *GenAiDllPath);
        return false;
    }

    try
    {
        std::string const ModelPathUtf8 = TCHAR_TO_UTF8(*FullModelPath);
        mImpl->mModel = OgaModel::Create(ModelPathUtf8.c_str());
        if (!mImpl->mModel)
        {
            return false;
        }

        mImpl->mTokenizer = OgaTokenizer::Create(*mImpl->mModel);
        if (!mImpl->mTokenizer)
        {
            mImpl->mModel.reset();
            return false;
        }

        mbIsInitialized = true;
        UE_LOG(LogTemp, Log, TEXT("Qwen2.5 On-Device 모델 초기화 성공: %s"), *FullModelPath);
        return true;
    }
    catch (std::exception const& Exception)
    {
        UE_LOG(LogTemp, Error, TEXT("Qwen2.5 On-Device 모델 초기화 실패: %s"), UTF8_TO_TCHAR(Exception.what()));
        Shutdown();
        return false;
    }
}

void FQwenOnDeviceRunner::Shutdown()
{
    if (!mImpl)
    {
        mbIsInitialized = false;
        return;
    }

    std::lock_guard<std::mutex> Lock(mImpl->mInferenceMutex);
    mImpl->mTokenizer.reset();
    mImpl->mModel.reset();
    if (mImpl->mGenAiDllHandle != nullptr)
    {
        FPlatformProcess::FreeDllHandle(mImpl->mGenAiDllHandle);
        mImpl->mGenAiDllHandle = nullptr;
    }
    if (mImpl->mOrtDllHandle != nullptr)
    {
        FPlatformProcess::FreeDllHandle(mImpl->mOrtDllHandle);
        mImpl->mOrtDllHandle = nullptr;
    }
    mbIsInitialized = false;
}

bool FQwenOnDeviceRunner::IsInitialized() const
{
    return mbIsInitialized;
}

FString FQwenOnDeviceRunner::GenerateText(FString const& Prompt, int32 const MaxTokens)
{
    if (!mbIsInitialized || !mImpl || !mImpl->mModel || !mImpl->mTokenizer)
    {
        return FString();
    }

    if (Prompt.IsEmpty())
    {
        return FString();
    }

    std::lock_guard<std::mutex> Lock(mImpl->mInferenceMutex);

    try
    {
        std::string const PromptUtf8 = TCHAR_TO_UTF8(*Prompt);
        std::unique_ptr<OgaSequences> InputSequences = OgaSequences::Create();
        mImpl->mTokenizer->Encode(PromptUtf8.c_str(), *InputSequences);

        size_t const PromptTokenCount = InputSequences->SequenceCount(0);
        std::unique_ptr<OgaGeneratorParams> Params = OgaGeneratorParams::Create(*mImpl->mModel);
        Params->SetSearchOption("max_length", static_cast<double>(PromptTokenCount + FMath::Max(1, MaxTokens)));
        Params->SetSearchOption("batch_size", 1.0);

        std::unique_ptr<OgaGenerator> Generator = OgaGenerator::Create(*mImpl->mModel, *Params);
        Generator->AppendTokenSequences(*InputSequences);

        while (!Generator->IsDone())
        {
            Generator->GenerateNextToken();
        }

        size_t const OutputSequenceLength = Generator->GetSequenceCount(0);
        int32_t const* OutputTokens = Generator->GetSequenceData(0);

        if (OutputTokens == nullptr || OutputSequenceLength <= PromptTokenCount)
        {
            return FString();
        }

        int32_t const* GeneratedTokens = OutputTokens + PromptTokenCount;
        size_t const GeneratedCount = OutputSequenceLength - PromptTokenCount;

        OgaString DecodedString = mImpl->mTokenizer->Decode(GeneratedTokens, GeneratedCount);
        FString Result = UTF8_TO_TCHAR(static_cast<char const*>(DecodedString));
        Result.TrimStartAndEndInline();
        return Result;
    }
    catch (std::exception const& Exception)
    {
        UE_LOG(LogTemp, Error, TEXT("Qwen 추론 실행 중 예외 발생: %s"), UTF8_TO_TCHAR(Exception.what()));
        return FString();
    }
}
