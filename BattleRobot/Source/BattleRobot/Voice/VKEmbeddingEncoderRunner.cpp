#include "VKEmbeddingEncoderRunner.h"

#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include "onnxruntime_c_api.h"
#include "Windows/HideWindowsPlatformTypes.h"

#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformProcess.h"
#include <mutex>
#include <string>

struct FVKEmbeddingEncoderRunner::FImpl
{
    OrtApi const* mApi = nullptr;
    OrtEnv* mEnv = nullptr;
    OrtSession* mSession = nullptr;
    OrtMemoryInfo* mMemInfo = nullptr;
    void* mOrtDllHandle = nullptr;

    TMap<FString, int32> mVocab;
    int32 mClsTokenId = 0;
    int32 mSepTokenId = 2;
    int32 mUnkTokenId = 3;
    int32 mPadTokenId = 1;
    int32 mHiddenDim = 768;

    std::mutex mInferenceMutex;

    void EncodeWordPiece(FString const& InText, TArray<int64>& OutIds, TArray<int64>& OutMask) const
    {
        OutIds.Empty();
        OutMask.Empty();

        OutIds.Add(static_cast<int64>(mClsTokenId));
        OutMask.Add(1);

        TArray<FString> Words;
        InText.ParseIntoArrayWS(Words);

        for (FString const& RawWord : Words)
        {
            FString Word = RawWord.ToLower();
            if (Word.IsEmpty())
            {
                continue;
            }

            int32 StartIndex = 0;
            int32 const WordLength = Word.Len();

            while (StartIndex < WordLength)
            {
                int32 EndIndex = WordLength;
                int32 MatchedId = -1;
                int32 MatchedLen = 0;

                while (EndIndex > StartIndex)
                {
                    int32 const CurrentSubLen = EndIndex - StartIndex;
                    FString SubPiece = Word.Mid(StartIndex, CurrentSubLen);
                    if (StartIndex > 0)
                    {
                        SubPiece = FString(TEXT("##")) + SubPiece;
                    }

                    int32 const* FoundId = mVocab.Find(SubPiece);
                    if (FoundId != nullptr)
                    {
                        MatchedId = *FoundId;
                        MatchedLen = CurrentSubLen;
                        break;
                    }

                    EndIndex--;
                }

                if (MatchedId == -1)
                {
                    OutIds.Add(static_cast<int64>(mUnkTokenId));
                    OutMask.Add(1);
                    break;
                }

                OutIds.Add(static_cast<int64>(MatchedId));
                OutMask.Add(1);
                StartIndex += MatchedLen;
            }
        }

        OutIds.Add(static_cast<int64>(mSepTokenId));
        OutMask.Add(1);
    }
};

FVKEmbeddingEncoderRunner::FVKEmbeddingEncoderRunner()
    : mImpl(MakeUnique<FImpl>())
    , mbIsInitialized(false)
{
}

FVKEmbeddingEncoderRunner::~FVKEmbeddingEncoderRunner()
{
    Shutdown();
}

bool FVKEmbeddingEncoderRunner::Initialize(FString const& ModelFolderPath)
{
    if (mbIsInitialized)
    {
        return true;
    }

    if (ModelFolderPath.IsEmpty() || !mImpl)
    {
        return false;
    }

    FString const FullModelDir = FPaths::ConvertRelativePathToFull(ModelFolderPath);
    FString const ModelFilePath = FPaths::Combine(FullModelDir, TEXT("model.onnx"));
    FString const VocabFilePath = FPaths::Combine(FullModelDir, TEXT("vocab.txt"));

    if (!FPaths::FileExists(ModelFilePath) || !FPaths::FileExists(VocabFilePath))
    {
        return false;
    }

    TArray<FString> VocabLines;
    if (!FFileHelper::LoadFileToStringArray(VocabLines, *VocabFilePath))
    {
        return false;
    }

    mImpl->mVocab.Empty(VocabLines.Num());
    for (int32 Index = 0; Index < VocabLines.Num(); ++Index)
    {
        FString const TrimmedLine = VocabLines[Index].TrimStartAndEnd();
        mImpl->mVocab.Add(TrimmedLine, Index);
    }

    if (int32 const* ClsId = mImpl->mVocab.Find(TEXT("[CLS]")))
    {
        mImpl->mClsTokenId = *ClsId;
    }
    if (int32 const* SepId = mImpl->mVocab.Find(TEXT("[SEP]")))
    {
        mImpl->mSepTokenId = *SepId;
    }
    if (int32 const* UnkId = mImpl->mVocab.Find(TEXT("[UNK]")))
    {
        mImpl->mUnkTokenId = *UnkId;
    }
    if (int32 const* PadId = mImpl->mVocab.Find(TEXT("[PAD]")))
    {
        mImpl->mPadTokenId = *PadId;
    }

    FString OrtDllPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Binaries/Win64/onnxruntime.dll"));
    if (!FPaths::FileExists(OrtDllPath))
    {
        OrtDllPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/ThirdParty/Onnx/Lib/onnxruntime.dll"));
    }
    mImpl->mOrtDllHandle = FPlatformProcess::GetDllHandle(*OrtDllPath);

    OrtApiBase const* ApiBase = OrtGetApiBase();
    if (ApiBase == nullptr)
    {
        return false;
    }

    mImpl->mApi = ApiBase->GetApi(ORT_API_VERSION);
    if (mImpl->mApi == nullptr)
    {
        return false;
    }

    OrtStatus* Status = mImpl->mApi->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "VKEmbeddingEncoder", &mImpl->mEnv);
    if (Status != nullptr)
    {
        mImpl->mApi->ReleaseStatus(Status);
        return false;
    }

    OrtSessionOptions* SessionOptions = nullptr;
    Status = mImpl->mApi->CreateSessionOptions(&SessionOptions);
    if (Status != nullptr)
    {
        mImpl->mApi->ReleaseStatus(Status);
        Shutdown();
        return false;
    }

    mImpl->mApi->SetIntraOpNumThreads(SessionOptions, 2);
    mImpl->mApi->SetSessionGraphOptimizationLevel(SessionOptions, ORT_ENABLE_ALL);

#if PLATFORM_WINDOWS
    Status = mImpl->mApi->CreateSession(mImpl->mEnv, *ModelFilePath, SessionOptions, &mImpl->mSession);
#else
    std::string const ModelPathUtf8 = TCHAR_TO_UTF8(*ModelFilePath);
    Status = mImpl->mApi->CreateSession(mImpl->mEnv, ModelPathUtf8.c_str(), SessionOptions, &mImpl->mSession);
#endif
    mImpl->mApi->ReleaseSessionOptions(SessionOptions);

    if (Status != nullptr)
    {
        mImpl->mApi->ReleaseStatus(Status);
        Shutdown();
        return false;
    }

    Status = mImpl->mApi->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &mImpl->mMemInfo);
    if (Status != nullptr)
    {
        mImpl->mApi->ReleaseStatus(Status);
        Shutdown();
        return false;
    }

    mbIsInitialized = true;
    return true;
}

void FVKEmbeddingEncoderRunner::Shutdown()
{
    if (!mImpl)
    {
        mbIsInitialized = false;
        return;
    }

    std::lock_guard<std::mutex> Lock(mImpl->mInferenceMutex);

    if (mImpl->mApi != nullptr)
    {
        if (mImpl->mMemInfo != nullptr)
        {
            mImpl->mApi->ReleaseMemoryInfo(mImpl->mMemInfo);
            mImpl->mMemInfo = nullptr;
        }

        if (mImpl->mSession != nullptr)
        {
            mImpl->mApi->ReleaseSession(mImpl->mSession);
            mImpl->mSession = nullptr;
        }

        if (mImpl->mEnv != nullptr)
        {
            mImpl->mApi->ReleaseEnv(mImpl->mEnv);
            mImpl->mEnv = nullptr;
        }

        mImpl->mApi = nullptr;
    }

    if (mImpl->mOrtDllHandle != nullptr)
    {
        FPlatformProcess::FreeDllHandle(mImpl->mOrtDllHandle);
        mImpl->mOrtDllHandle = nullptr;
    }

    mImpl->mVocab.Empty();
    mbIsInitialized = false;
}

bool FVKEmbeddingEncoderRunner::IsInitialized() const
{
    return mbIsInitialized;
}

bool FVKEmbeddingEncoderRunner::GetSentenceEmbedding(FString const& Text, TArray<float>& OutEmbedding)
{
    OutEmbedding.Empty();

    if (!mbIsInitialized || !mImpl || mImpl->mApi == nullptr || mImpl->mSession == nullptr || mImpl->mMemInfo == nullptr)
    {
        return false;
    }

    FString TrimmedText = Text;
    TrimmedText.TrimStartAndEndInline();
    if (TrimmedText.IsEmpty())
    {
        return false;
    }

    TArray<int64> InputIds;
    TArray<int64> AttentionMask;
    mImpl->EncodeWordPiece(TrimmedText, InputIds, AttentionMask);

    int32 const SequenceLength = InputIds.Num();
    if (SequenceLength <= 0)
    {
        return false;
    }

    std::lock_guard<std::mutex> Lock(mImpl->mInferenceMutex);

    int64_t const InputShape[2] = { 1, SequenceLength };
    size_t const ElementCount = static_cast<size_t>(SequenceLength);

    OrtValue* InputTensors[2] = { nullptr, nullptr };

    OrtStatus* Status = mImpl->mApi->CreateTensorWithDataAsOrtValue(
        mImpl->mMemInfo,
        InputIds.GetData(),
        ElementCount * sizeof(int64),
        InputShape,
        2,
        ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64,
        &InputTensors[0]);

    if (Status != nullptr)
    {
        mImpl->mApi->ReleaseStatus(Status);
        return false;
    }

    Status = mImpl->mApi->CreateTensorWithDataAsOrtValue(
        mImpl->mMemInfo,
        AttentionMask.GetData(),
        ElementCount * sizeof(int64),
        InputShape,
        2,
        ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64,
        &InputTensors[1]);

    if (Status != nullptr)
    {
        mImpl->mApi->ReleaseStatus(Status);
        mImpl->mApi->ReleaseValue(InputTensors[0]);
        return false;
    }

    char const* InputNames[2] = { "input_ids", "attention_mask" };
    char const* OutputNames[1] = { "last_hidden_state" };
    OrtValue* OutputTensor = nullptr;

    Status = mImpl->mApi->Run(
        mImpl->mSession,
        nullptr,
        InputNames,
        InputTensors,
        2,
        OutputNames,
        1,
        &OutputTensor);

    mImpl->mApi->ReleaseValue(InputTensors[0]);
    mImpl->mApi->ReleaseValue(InputTensors[1]);

    if (Status != nullptr)
    {
        mImpl->mApi->ReleaseStatus(Status);
        return false;
    }

    if (OutputTensor == nullptr)
    {
        return false;
    }

    float* HiddenStateData = nullptr;
    Status = mImpl->mApi->GetTensorMutableData(OutputTensor, reinterpret_cast<void**>(&HiddenStateData));
    if (Status != nullptr || HiddenStateData == nullptr)
    {
        if (Status != nullptr)
        {
            mImpl->mApi->ReleaseStatus(Status);
        }
        mImpl->mApi->ReleaseValue(OutputTensor);
        return false;
    }

    int32 const HiddenDim = mImpl->mHiddenDim;
    OutEmbedding.SetNumUninitialized(HiddenDim);

    float NormAccumulator = 0.0f;
    for (int32 DimIdx = 0; DimIdx < HiddenDim; ++DimIdx)
    {
        float Sum = 0.0f;
        for (int32 SeqIdx = 0; SeqIdx < SequenceLength; ++SeqIdx)
        {
            Sum += HiddenStateData[SeqIdx * HiddenDim + DimIdx];
        }

        float const MeanVal = Sum / static_cast<float>(SequenceLength);
        OutEmbedding[DimIdx] = MeanVal;
        NormAccumulator += MeanVal * MeanVal;
    }

    mImpl->mApi->ReleaseValue(OutputTensor);

    float const InvNorm = (NormAccumulator > 1e-12f) ? FMath::InvSqrt(NormAccumulator) : 0.0f;
    for (int32 DimIdx = 0; DimIdx < HiddenDim; ++DimIdx)
    {
        OutEmbedding[DimIdx] *= InvNorm;
    }

    return true;
}
