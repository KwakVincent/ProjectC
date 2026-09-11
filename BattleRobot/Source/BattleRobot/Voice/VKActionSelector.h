#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "VKActionSelector.generated.h"

USTRUCT(BlueprintType)
struct FBotActionParseResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="VoiceAction")
    FName mActionId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="VoiceAction")
    FVector mDirection = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category="VoiceAction")
    int32 mDeltaTicks = 0;

    UPROPERTY(BlueprintReadOnly, Category="VoiceAction")
    bool mbSuccess = false;
};

class UVKActionPoolComponent;
class FVKEmbeddingEncoderRunner;

UCLASS(BlueprintType)
class BATTLEROBOT_API UVKActionSelector : public UObject
{
    GENERATED_BODY()

public:
    UVKActionSelector();

    static FBotActionParseResult ClassifyVoiceCommand(
        FString const& RawVoiceText,
        UVKActionPoolComponent const* ActionPool,
        FVKEmbeddingEncoderRunner* EncoderRunner,
        float const SimilarityThreshold = 0.40f);

    UFUNCTION(BlueprintCallable, Category="VoiceAction")
    static FVector ExtractDirectionFromText(FString const& Text, FName const& ActionId);

    UFUNCTION(BlueprintCallable, Category="VoiceAction")
    static int32 ExtractDeltaTicksFromText(FString const& Text);

    UFUNCTION(BlueprintCallable, Category="VoiceAction")
    static FString BuildPrompt(FString const& UserVoiceText, UVKActionPoolComponent const* ActionPool);

    UFUNCTION(BlueprintCallable, Category="VoiceAction")
    static bool ParseTokenResponse(
        FString const& ResponseText,
        UVKActionPoolComponent const* ActionPool,
        FName& OutActionId,
        FVector& OutDirection,
        int32& OutDeltaTicks);

    UFUNCTION(BlueprintCallable, Category="VoiceAction")
    static FBotActionParseResult ParseTokens(FString const& ResponseText, UVKActionPoolComponent const* ActionPool);

    UFUNCTION(BlueprintCallable, Category="VoiceAction")
    static FVector ConvertDirectionToVector(FString const& DirectionString);
};
