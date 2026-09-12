#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VoiceCommandClassifier.h"
#include "VoicePipeline.h"
#include "VKEmbeddingEncoderRunner.h"
#include "VKActionSelector.h"
#include "VKVoiceCommanderComponent.generated.h"

class ABattleRobotCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVoiceCommandExecuted, EBotVoiceCommand, Command, FString const&, RawText);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnActionTrained, FName, ActionId, float, Speed, float, Duration, bool, bKeepUntilStop);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActionExecutionImperfect, FName, ActionId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVoiceCommandUnrecognized, FString const&, RawText);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class BATTLEROBOT_API UVKVoiceCommanderComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UVKVoiceCommanderComponent();
    virtual ~UVKVoiceCommanderComponent() override;

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintCallable, Category="Voice")
    void StartVoiceRecognition();

    UFUNCTION(BlueprintCallable, Category="Voice")
    void StopVoiceRecognition();

    UFUNCTION(BlueprintCallable, Category="Voice")
    void RestartVoiceRecognition();

    UFUNCTION(BlueprintCallable, Category="Voice")
    bool IsMicrophoneCapturing() const;

    UFUNCTION(BlueprintCallable, Category="Voice")
    void SetTrainingMode(bool const bEnable);

    UFUNCTION(BlueprintCallable, Category="Voice")
    bool IsTrainingMode() const;

    UFUNCTION(BlueprintCallable, Category="Voice")
    class UVKActionPoolComponent* GetActionPool() const;

    UFUNCTION(BlueprintCallable, Category="Voice")
    void SetConsecutiveActionLevel(int32 const NewLevel);

    UFUNCTION(BlueprintCallable, Category="Voice")
    int32 GetConsecutiveActionLevel() const;

    UFUNCTION(BlueprintCallable, Category="Voice")
    void SetSimilarityThreshold(float const NewThreshold);

    UFUNCTION(BlueprintCallable, Category="Voice")
    float GetSimilarityThreshold() const;

    UFUNCTION(BlueprintCallable, Category="Voice")
    void SetBufferRetentionTimeout(float const NewTimeoutSec);

    UFUNCTION(BlueprintCallable, Category="Voice")
    float GetBufferRetentionTimeout() const;

    UFUNCTION(BlueprintCallable, Category="Voice")
    class UVKActiveActionComponent* GetActiveAction() const;

    FVKEmbeddingEncoderRunner* GetEncoderRunner() const;

    UPROPERTY(BlueprintAssignable, Category="Voice")
    FOnVoiceCommandExecuted OnVoiceCommandExecuted;

    UPROPERTY(BlueprintAssignable, Category="Voice")
    FOnActionTrained OnActionTrained;

    UPROPERTY(BlueprintAssignable, Category="Voice")
    FOnActionExecutionImperfect OnActionExecutionImperfect;

    UPROPERTY(BlueprintAssignable, Category="Voice")
    FOnVoiceCommandUnrecognized OnVoiceCommandUnrecognized;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type const EndPlayReason) override;

    void HandleSpeechRecognized(FString const& RecognizedText, double VadDurationMs, double SttDurationMs);
    void HandleCommandUnrecognized(FString const& RawText);
    void ExecuteCommandOnGameThread(EBotVoiceCommand Command, FString const& RawText);
    void ExecuteActionOnGameThread(
        FName const& ActionId,
        FVector const& Direction,
        int32 DeltaTicks,
        FString const& RawText,
        double VadDurationMs,
        double SttDurationMs,
        double InferDurationMs,
        double DispatchStartTime);

    void ExecuteActionSequenceOnGameThread(
        TArray<FBotActionParseResult> const& ParseResults,
        FString const& RawText,
        double VadDurationMs,
        double SttDurationMs,
        double InferDurationMs,
        double DispatchStartTime);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voice")
    float mMoveDuration;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voice", meta=(ClampMin="1", ClampMax="10"))
    int32 mMaxConsecutiveActions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voice", meta=(ClampMin="0.0", ClampMax="1.0"))
    float mSimilarityThreshold;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voice", meta=(ClampMin="0.1", ClampMax="3.0"))
    float mBufferRetentionTimeout;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Voice")
    TObjectPtr<class UVKActionPoolComponent> mActionPool;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Voice")
    TObjectPtr<class UVKActiveActionComponent> mActiveAction;

    TUniquePtr<FVoicePipeline> mVoicePipeline;
    TUniquePtr<FVKEmbeddingEncoderRunner> mEncoderRunner;
    TUniquePtr<IVoiceCommandClassifier> mClassifier;
    TWeakObjectPtr<ABattleRobotCharacter> mOwnerCharacter;
    EBotVoiceCommand mActiveCommand;
    float mRemainingMoveTime;
    float mMicrophoneRetryTimer = 0.0f;

    bool mbIsTrainingMode = false;
    bool mbIsContinuousMoving = false;
    FVector mContinuousDirection = FVector::ZeroVector;
    float mContinuousSpeedMultiplier = 1.0f;
};
