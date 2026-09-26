#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VKActionPoolComponent.generated.h"

USTRUCT(BlueprintType)
struct FBotMovementActionDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action")
    FName mActionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action")
    FString mDisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action")
    FString mDescription;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action")
    float mSpeedMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action")
    float mDefaultDuration = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action")
    float mAdaptiveDuration = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action")
    bool mbIsUnlocked = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action")
    bool mbCanTurn = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action")
    TArray<FString> mTriggerPhrases;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action")
    float mProficiency = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action")
    int32 mSuccessfulUses = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action")
    bool mbKeepUntilStop = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action")
    float mTargetSpeedMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action")
    float mTargetDuration = 1.0f;
};

USTRUCT(BlueprintType)
struct FVoiceCommandMapping
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Command")
	FName mCommandId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Command")
	TArray<FString> mTriggerPhrases;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Command")
	FName mTargetActionId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Command")
	FVector mDirection = FVector(1.0f, 0.0f, 0.0f);
};

struct FActionPhraseEmbedding
{
	FString mPhrase;
	TArray<float> mVector;
};

struct FActionAnchorEntry
{
	FName mActionId;
	TArray<FActionPhraseEmbedding> mPhraseEmbeddings;
};

struct FCommandAnchorEntry
{
	FName mCommandId;
	FName mTargetActionId;
	FVector mDirection = FVector::ZeroVector;
	TArray<FActionPhraseEmbedding> mPhraseEmbeddings;
};

class FVKEmbeddingEncoderRunner;
class UVKActionDataAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActionPoolChanged, FName const&, ActionId);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class BATTLEROBOT_API UVKActionPoolComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UVKActionPoolComponent();

    UFUNCTION(BlueprintCallable, Category="ActionPool")
    bool RegisterAction(FBotMovementActionDef const& NewAction);

    UFUNCTION(BlueprintCallable, Category="ActionPool")
    bool RegisterCustomAction(FBotMovementActionDef const& NewAction, TArray<FString> const& TriggerPhrases);

    UFUNCTION(BlueprintCallable, Category="ActionPool")
    bool AddTriggerPhrase(FName const& ActionId, FString const& NewPhrase);

    UFUNCTION(BlueprintCallable, Category="ActionPool")
    bool RemoveTriggerPhrase(FName const& ActionId, FString const& Phrase);

    UFUNCTION(BlueprintCallable, Category="ActionPool")
    TArray<FString> GetTriggerPhrases(FName const& ActionId) const;

    UFUNCTION(BlueprintCallable, Category="ActionPool")
    bool UnregisterAction(FName const& ActionId);

    UFUNCTION(BlueprintCallable, Category="ActionPool")
    bool UnlockAction(FName const& ActionId);

    UFUNCTION(BlueprintCallable, Category="ActionPool")
    bool LockAction(FName const& ActionId);

    UFUNCTION(BlueprintCallable, Category="ActionPool")
    bool IsActionUnlocked(FName const& ActionId) const;

    UFUNCTION(BlueprintCallable, Category="ActionPool")
    float AdjustActionDuration(FName const& ActionId, int32 DeltaTicks);

    UFUNCTION(BlueprintCallable, Category="ActionPool")
    bool GetAction(FName const& ActionId, FBotMovementActionDef& OutAction) const;

    UFUNCTION(BlueprintCallable, Category="ActionPool")
    TArray<FBotMovementActionDef> GetAllActions() const;

    UFUNCTION(BlueprintCallable, Category="ActionPool")
    FString BuildLLMActionCatalog() const;

    UFUNCTION(BlueprintCallable, Category="ActionPool")
    void ResetToDefaultActions();

    void CacheActionEmbeddings(FVKEmbeddingEncoderRunner* EncoderRunner);

    UFUNCTION(BlueprintCallable, Category="ActionPool")
    void LoadFromDataAsset(UVKActionDataAsset const* DataAsset);

    bool FindBestMatchingCommand(
        TArray<float> const& QueryEmbedding, float const SimilarityThreshold,
        FName& OutActionId, FVector& OutDirection, float& OutSimilarity) const;

    bool FindBestMatchingAction(TArray<float> const& QueryEmbedding, float const SimilarityThreshold, FName& OutActionId, float& OutSimilarity) const;

    UFUNCTION(BlueprintCallable, Category="ActionPool")
    TArray<FVoiceCommandMapping> const& GetCommandMappings() const;

    UFUNCTION(BlueprintCallable, Category="ActionPool")
    bool EvaluateActionPerformance(FName const& ActionId, float& OutSpeed, float& OutDuration, bool& bOutKeepUntilStop, bool& bOutCriticalFail) const;

    void RecordActionSuccess(FName const& ActionId, TArray<float> const& UtteranceEmbedding);

    UPROPERTY(BlueprintAssignable, Category="ActionPool")
    FOnActionPoolChanged OnActionRegistered;

    UPROPERTY(BlueprintAssignable, Category="ActionPool")
    FOnActionPoolChanged OnActionUnregistered;

protected:
    virtual void BeginPlay() override;

    void InitializeDefaultActions();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ActionPool")
    TObjectPtr<UVKActionDataAsset> mActionDataAsset;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ActionPool")
    TMap<FName, FBotMovementActionDef> mActionMap;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ActionPool")
    TArray<FVoiceCommandMapping> mCommandMappings;

    TMap<FName, FActionAnchorEntry> mAnchorCache;
    TMap<FName, FCommandAnchorEntry> mCommandAnchorCache;
};
