#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VoiceCommandClassifier.h"
#include "VoicePipeline.h"
#include "VKEmbeddingEncoderRunner.h"
#include "VKVoiceCommanderComponent.generated.h"

class ABattleRobotCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVoiceCommandExecuted, EBotVoiceCommand, Command, FString const&, RawText);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnActionTrained, FName, ActionId, float, Speed, float, Duration, bool, bKeepUntilStop);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActionExecutionImperfect, FName, ActionId);

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
    void SetTrainingMode(bool const bEnable);

    UFUNCTION(BlueprintCallable, Category="Voice")
    bool IsTrainingMode() const;

    UFUNCTION(BlueprintCallable, Category="Voice")
    class UVKActionPoolComponent* GetActionPool() const;

    UFUNCTION(BlueprintCallable, Category="Voice")
    class UVKActiveActionComponent* GetActiveAction() const;

    FVKEmbeddingEncoderRunner* GetEncoderRunner() const;

    UPROPERTY(BlueprintAssignable, Category="Voice")
    FOnVoiceCommandExecuted OnVoiceCommandExecuted;

    UPROPERTY(BlueprintAssignable, Category="Voice")
    FOnActionTrained OnActionTrained;

    UPROPERTY(BlueprintAssignable, Category="Voice")
    FOnActionExecutionImperfect OnActionExecutionImperfect;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type const EndPlayReason) override;

    void HandleSpeechRecognized(FString const& RecognizedText, double VadDurationMs, double SttDurationMs);
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voice")
    float mMoveDuration;

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

    bool mbIsTrainingMode = false;
    bool mbIsContinuousMoving = false;
    FVector mContinuousDirection = FVector::ZeroVector;
    float mContinuousSpeedMultiplier = 1.0f;
};
