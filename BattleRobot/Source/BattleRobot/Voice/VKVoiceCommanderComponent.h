#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VoiceCommandClassifier.h"
#include "VoicePipeline.h"
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

    UPROPERTY(BlueprintAssignable, Category="Voice")
    FOnVoiceCommandExecuted OnVoiceCommandExecuted;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type const EndPlayReason) override;

    void HandleSpeechRecognized(FString const& RecognizedText);
    void ExecuteCommandOnGameThread(EBotVoiceCommand Command, FString const& RawText);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Voice")
    float mMoveDuration;

    TUniquePtr<FVoicePipeline> mVoicePipeline;
    TUniquePtr<IVoiceCommandClassifier> mClassifier;
    TWeakObjectPtr<ABattleRobotCharacter> mOwnerCharacter;

    EBotVoiceCommand mActiveCommand;
    float mRemainingMoveTime;
};
