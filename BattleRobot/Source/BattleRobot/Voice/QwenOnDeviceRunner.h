#pragma once

#include "CoreMinimal.h"
#include "Templates/UniquePtr.h"

class BATTLEROBOT_API FQwenOnDeviceRunner
{
public:
    FQwenOnDeviceRunner();
    ~FQwenOnDeviceRunner();

    bool Initialize(FString const& ModelFolderPath);
    void Shutdown();
    bool IsInitialized() const;
    FString GenerateText(FString const& Prompt, int32 const MaxTokens = 10);

protected:
    struct FImpl;
    TUniquePtr<FImpl> mImpl;
    bool mbIsInitialized;
};
