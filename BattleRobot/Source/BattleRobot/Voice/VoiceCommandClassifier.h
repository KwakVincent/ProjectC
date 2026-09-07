#pragma once

#include "CoreMinimal.h"
#include "VoiceCommandClassifier.generated.h"

UENUM(BlueprintType)
enum class EBotVoiceCommand : uint8
{
    None        UMETA(DisplayName = "None"),
    Forward     UMETA(DisplayName = "Forward"),
    Backward    UMETA(DisplayName = "Backward"),
    Left        UMETA(DisplayName = "Left"),
    Right       UMETA(DisplayName = "Right"),
    Stop        UMETA(DisplayName = "Stop")
};

class BATTLEROBOT_API IVoiceCommandClassifier
{
public:
    virtual ~IVoiceCommandClassifier() = default;
    virtual EBotVoiceCommand ClassifyCommand(FString const& RecognizedText) = 0;
};

class BATTLEROBOT_API FRuleBasedCommandClassifier : public IVoiceCommandClassifier
{
public:
    FRuleBasedCommandClassifier();
    virtual ~FRuleBasedCommandClassifier() override = default;

    virtual EBotVoiceCommand ClassifyCommand(FString const& RecognizedText) override;

protected:
    bool ContainsAny(FString const& Text, TArray<FString> const& Keywords) const;

    TArray<FString> mForwardKeywords;
    TArray<FString> mBackwardKeywords;
    TArray<FString> mLeftKeywords;
    TArray<FString> mRightKeywords;
    TArray<FString> mStopKeywords;
};
