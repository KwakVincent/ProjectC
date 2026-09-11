#pragma once

#include "CoreMinimal.h"
#include "Templates/UniquePtr.h"

class BATTLEROBOT_API FVKEmbeddingEncoderRunner
{
public:
    FVKEmbeddingEncoderRunner();
    ~FVKEmbeddingEncoderRunner();

    bool Initialize(FString const& ModelFolderPath);
    void Shutdown();
    bool IsInitialized() const;

    bool GetSentenceEmbedding(FString const& Text, TArray<float>& OutEmbedding);

protected:
    struct FImpl;
    TUniquePtr<FImpl> mImpl;
    bool mbIsInitialized;
};
