#include "VKActiveActionComponent.h"

UVKActiveActionComponent::UVKActiveActionComponent()
    : mRemainingDuration(0.0f)
    , mbIsActive(false)
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UVKActiveActionComponent::ApplyAction(FName const& ActionId, FVector const& Direction, float SpeedMultiplier, float Duration)
{
    ClearQueue();

    if (ActionId.IsNone())
    {
        StopAction();
        return;
    }

    FBotActionCommand Command;
    Command.mActionId = ActionId;
    Command.mDirection = Direction.GetSafeNormal();
    Command.mSpeedMultiplier = FMath::Max(0.0f, SpeedMultiplier);
    Command.mDuration = FMath::Max(0.0f, Duration);

    EnqueueAction(Command);
}

void UVKActiveActionComponent::EnqueueAction(FBotActionCommand const& Command)
{
    if (Command.mActionId.IsNone())
    {
        return;
    }

    mActionQueue.Add(Command);

    if (!mbIsActive)
    {
        PlayNextQueuedAction();
    }
}

void UVKActiveActionComponent::EnqueueActionSequence(TArray<FBotActionCommand> const& Sequence)
{
    if (Sequence.IsEmpty())
    {
        return;
    }

    for (FBotActionCommand const& Command : Sequence)
    {
        if (Command.mActionId.IsNone())
        {
            continue;
        }

        mActionQueue.Add(Command);
    }

    if (!mbIsActive)
    {
        PlayNextQueuedAction();
    }
}

void UVKActiveActionComponent::ClearQueue()
{
    mActionQueue.Empty();
}

void UVKActiveActionComponent::StopAction()
{
    ClearQueue();

    if (!mbIsActive && mCurrentCommand.mActionId.IsNone())
    {
        return;
    }

    FName const FinishedActionId = mCurrentCommand.mActionId;
    mCurrentCommand = FBotActionCommand();
    mRemainingDuration = 0.0f;
    mbIsActive = false;

    OnActionFinished.Broadcast(FinishedActionId);
}

void UVKActiveActionComponent::PlayNextQueuedAction()
{
    if (mActionQueue.IsEmpty())
    {
        mCurrentCommand = FBotActionCommand();
        mRemainingDuration = 0.0f;
        mbIsActive = false;
        return;
    }

    mCurrentCommand = mActionQueue[0];
    mActionQueue.RemoveAt(0);

    mRemainingDuration = mCurrentCommand.mDuration;
    mbIsActive = (mRemainingDuration > 0.0f || mCurrentCommand.mSpeedMultiplier > 0.0f);

    OnActionStarted.Broadcast(mCurrentCommand.mActionId);
}

bool UVKActiveActionComponent::IsActionActive() const
{
    return mbIsActive;
}

int32 UVKActiveActionComponent::GetQueuedActionCount() const
{
    return mActionQueue.Num();
}

FName UVKActiveActionComponent::GetCurrentActionId() const
{
    return mCurrentCommand.mActionId;
}

FVector UVKActiveActionComponent::GetCurrentDirection() const
{
    return mCurrentCommand.mDirection;
}

float UVKActiveActionComponent::GetCurrentSpeedMultiplier() const
{
    return mCurrentCommand.mSpeedMultiplier;
}

float UVKActiveActionComponent::GetRemainingDuration() const
{
    return mRemainingDuration;
}

void UVKActiveActionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!mbIsActive)
    {
        return;
    }

    mRemainingDuration -= DeltaTime;
    if (mRemainingDuration <= 0.0f)
    {
        FName const FinishedActionId = mCurrentCommand.mActionId;
        OnActionFinished.Broadcast(FinishedActionId);

        PlayNextQueuedAction();
    }
}
