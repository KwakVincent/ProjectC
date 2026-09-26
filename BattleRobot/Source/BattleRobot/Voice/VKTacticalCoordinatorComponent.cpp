#include "VKTacticalCoordinatorComponent.h"
#include "VKTargetPerceiverComponent.h"
#include "VKNavPathPlannerComponent.h"
#include "VKActiveActionComponent.h"
#include "BattleRobotCharacter.h"
#include "GameFramework/Controller.h"

UVKTacticalCoordinatorComponent::UVKTacticalCoordinatorComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UVKTacticalCoordinatorComponent::BeginPlay()
{
    Super::BeginPlay();

    AActor* OwnerActor = GetOwner();
    if (OwnerActor == nullptr)
    {
        return;
    }

    mOwnerCharacter = Cast<ABattleRobotCharacter>(OwnerActor);

    mTargetPerceiver = OwnerActor->FindComponentByClass<UVKTargetPerceiverComponent>();
    if (!mTargetPerceiver.IsValid())
    {
        UVKTargetPerceiverComponent* NewPerceiver = NewObject<UVKTargetPerceiverComponent>(OwnerActor, TEXT("TargetPerceiver"));
        NewPerceiver->RegisterComponent();
        mTargetPerceiver = NewPerceiver;
    }

    mNavPathPlanner = OwnerActor->FindComponentByClass<UVKNavPathPlannerComponent>();
    if (!mNavPathPlanner.IsValid())
    {
        UVKNavPathPlannerComponent* NewPlanner = NewObject<UVKNavPathPlannerComponent>(OwnerActor, TEXT("NavPathPlanner"));
        NewPlanner->RegisterComponent();
        mNavPathPlanner = NewPlanner;
    }

    mActiveAction = OwnerActor->FindComponentByClass<UVKActiveActionComponent>();

    if (mNavPathPlanner.IsValid())
    {
        mNavPathPlanner->OnDestinationReached.AddDynamic(this, &UVKTacticalCoordinatorComponent::HandleDestinationReached);
        mNavPathPlanner->OnPathCalculated.AddDynamic(this, &UVKTacticalCoordinatorComponent::HandlePathCalculated);
    }
}

bool UVKTacticalCoordinatorComponent::ExecuteApproachEnemy()
{
    if (!mTargetPerceiver.IsValid() || !mNavPathPlanner.IsValid())
    {
        OnApproachFailed.Broadcast(TEXT("필요 컴포넌트가 유효하지 않습니다"));
        return false;
    }

    AActor* BestTarget = mTargetPerceiver->AcquireBestTarget();
    if (BestTarget == nullptr)
    {
        StopTactics();
        OnApproachFailed.Broadcast(TEXT("타깃을 찾을 수 없습니다"));
        return false;
    }

    if (mActiveAction.IsValid() && mActiveAction->IsActionActive())
    {
        mActiveAction->StopAction();
    }

    bool const bPathStarted = mNavPathPlanner->SetTargetActor(BestTarget);
    if (!bPathStarted)
    {
        StopTactics();
        OnApproachFailed.Broadcast(TEXT("내비게이션 경로 생성 실패"));
        return false;
    }

    mbIsApproaching = true;
    OnApproachStarted.Broadcast(BestTarget);
    return true;
}

void UVKTacticalCoordinatorComponent::StopTactics()
{
    mbIsApproaching = false;

    if (mNavPathPlanner.IsValid())
    {
        mNavPathPlanner->StopNavigation();
    }
}

bool UVKTacticalCoordinatorComponent::IsExecutingTactics() const
{
    return mbIsApproaching;
}

AActor* UVKTacticalCoordinatorComponent::GetCurrentTacticalTarget() const
{
    if (!mTargetPerceiver.IsValid())
    {
        return nullptr;
    }

    return mTargetPerceiver->GetCurrentTarget();
}

void UVKTacticalCoordinatorComponent::HandleDestinationReached()
{
    mbIsApproaching = false;

    AActor* Target = (mTargetPerceiver.IsValid()) ? mTargetPerceiver->GetCurrentTarget() : nullptr;
    OnApproachCompleted.Broadcast(Target);
}

void UVKTacticalCoordinatorComponent::HandlePathCalculated(bool const bSuccess, int32 const WaypointCount)
{
    if (!bSuccess && mbIsApproaching)
    {
        StopTactics();
        OnApproachFailed.Broadcast(TEXT("경로를 탐색하지 못했습니다"));
    }
}

void UVKTacticalCoordinatorComponent::UpdateMovementTowardsWaypoint()
{
    if (!mbIsApproaching || !mNavPathPlanner.IsValid() || !mOwnerCharacter.IsValid())
    {
        return;
    }

    FVector NextWaypoint = FVector::ZeroVector;
    if (!mNavPathPlanner->GetCurrentWaypoint(NextWaypoint))
    {
        return;
    }

    FVector const CharacterLocation = mOwnerCharacter->GetActorLocation();
    FVector const DirectionWorld = (NextWaypoint - CharacterLocation).GetSafeNormal2D();
    if (DirectionWorld.IsNearlyZero())
    {
        return;
    }

    AController const* Controller = mOwnerCharacter->GetController();
    FRotator const YawRotation(
        0.0f,
        (Controller != nullptr) ? Controller->GetControlRotation().Yaw : mOwnerCharacter->GetActorRotation().Yaw,
        0.0f);

    FVector const ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
    FVector const RightDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

    float const ForwardInput = FVector::DotProduct(DirectionWorld, ForwardDir) * mApproachSpeedMultiplier;
    float const RightInput = FVector::DotProduct(DirectionWorld, RightDir) * mApproachSpeedMultiplier;

    mOwnerCharacter->DoMove(RightInput, ForwardInput);
}

void UVKTacticalCoordinatorComponent::TickComponent(
    float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!mbIsApproaching)
    {
        return;
    }

    UpdateMovementTowardsWaypoint();
}
