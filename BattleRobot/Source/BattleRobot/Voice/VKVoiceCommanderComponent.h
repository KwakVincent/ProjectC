#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VoiceCommandClassifier.h"
#include "VoicePipeline.h"
#include "QwenOnDeviceRunner.h"
#include "VKVoiceCommanderComponent.generated.h"

class ABattleRobotCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVoiceCommandExecuted, EBotVoiceCommand, Command, FString const&, RawText);

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
    class UVKActionPoolComponent* GetActionPool() const;

    UFUNCTION(BlueprintCallable, Category="Voice")
    class UVKActiveActionComponent* GetActiveAction() const;

    UPROPERTY(BlueprintAssignable, Category="Voice")
    FOnVoiceCommandExecuted OnVoiceCommandExecuted;

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
    TUniquePtr<FQwenOnDeviceRunner> mQwenRunner;
    TUniquePtr<IVoiceCommandClassifier> mClassifier;
    TWeakObjectPtr<ABattleRobotCharacter> mOwnerCharacter;

    EBotVoiceCommand mActiveCommand;
    float mRemainingMoveTime;
};
