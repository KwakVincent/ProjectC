#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VKActiveActionComponent.generated.h"

USTRUCT(BlueprintType)
struct FBotActionCommand
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ActionCommand")
    FName mActionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ActionCommand")
    FVector mDirection = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ActionCommand")
    float mSpeedMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ActionCommand")
    float mDuration = 1.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActiveActionStateChanged, FName const&, ActionId);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class BATTLEROBOT_API UVKActiveActionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UVKActiveActionComponent();

    UFUNCTION(BlueprintCallable, Category="ActionState")
    void ApplyAction(FName const& ActionId, FVector const& Direction, float SpeedMultiplier, float Duration);

    UFUNCTION(BlueprintCallable, Category="ActionState")
    void EnqueueAction(FBotActionCommand const& Command);

    UFUNCTION(BlueprintCallable, Category="ActionState")
    void EnqueueActionSequence(TArray<FBotActionCommand> const& Sequence);

    UFUNCTION(BlueprintCallable, Category="ActionState")
    void ClearQueue();

    UFUNCTION(BlueprintCallable, Category="ActionState")
    void StopAction();

    UFUNCTION(BlueprintCallable, Category="ActionState")
    bool IsActionActive() const;

    UFUNCTION(BlueprintCallable, Category="ActionState")
    int32 GetQueuedActionCount() const;

    UFUNCTION(BlueprintCallable, Category="ActionState")
    FName GetCurrentActionId() const;

    UFUNCTION(BlueprintCallable, Category="ActionState")
    FVector GetCurrentDirection() const;

    UFUNCTION(BlueprintCallable, Category="ActionState")
    float GetCurrentSpeedMultiplier() const;

    UFUNCTION(BlueprintCallable, Category="ActionState")
    float GetRemainingDuration() const;

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(BlueprintAssignable, Category="ActionState")
    FOnActiveActionStateChanged OnActionStarted;

    UPROPERTY(BlueprintAssignable, Category="ActionState")
    FOnActiveActionStateChanged OnActionFinished;

protected:
    void PlayNextQueuedAction();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ActionState")
    FBotActionCommand mCurrentCommand;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ActionState")
    TArray<FBotActionCommand> mActionQueue;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ActionState")
    float mRemainingDuration;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ActionState")
    bool mbIsActive;
};
