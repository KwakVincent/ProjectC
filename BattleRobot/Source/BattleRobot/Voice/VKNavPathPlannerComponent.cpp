#include "VKNavPathPlannerComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"

UVKNavPathPlannerComponent::UVKNavPathPlannerComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UVKNavPathPlannerComponent::BeginPlay()
{
    Super::BeginPlay();
}

UNavigationSystemV1* UVKNavPathPlannerComponent::GetNavigationSystem() const
{
    UWorld* CurrentWorld = GetWorld();
    if (CurrentWorld == nullptr)
    {
        return nullptr;
    }

    return FNavigationSystem::GetCurrent<UNavigationSystemV1>(CurrentWorld);
}

FVector UVKNavPathPlannerComponent::GetTargetDestination() const
{
    if (mbUseActorTarget && mTargetActor.IsValid())
    {
        return mTargetActor->GetActorLocation();
    }

    return mTargetLocation;
}

bool UVKNavPathPlannerComponent::SetTargetActor(AActor* TargetActor)
{
    if (TargetActor == nullptr)
    {
        StopNavigation();
        return false;
    }

    mTargetActor = TargetActor;
    mbUseActorTarget = true;
    mbIsNavigating = true;
    mRepathTimer = 0.0f;

    return RequestPathCalculation();
}

bool UVKNavPathPlannerComponent::SetTargetLocation(FVector const& TargetLocation)
{
    mTargetLocation = TargetLocation;
    mTargetActor.Reset();
    mbUseActorTarget = false;
    mbIsNavigating = true;
    mRepathTimer = 0.0f;

    return RequestPathCalculation();
}

void UVKNavPathPlannerComponent::StopNavigation()
{
    mbIsNavigating = false;
    mTargetActor.Reset();
    mWaypoints.Empty();
    mCurrentWaypointIndex = 0;
    mRepathTimer = 0.0f;
}

bool UVKNavPathPlannerComponent::RequestPathCalculation()
{
    AActor const* OwnerActor = GetOwner();
    UWorld* CurrentWorld = GetWorld();
    if (OwnerActor == nullptr || CurrentWorld == nullptr)
    {
        return false;
    }

    UNavigationSystemV1* NavSys = GetNavigationSystem();
    if (NavSys == nullptr)
    {
        return false;
    }

    FVector const StartLocation = OwnerActor->GetActorLocation();
    FVector const DestinationLocation = GetTargetDestination();

    UNavigationPath* CalculatedPath = NavSys->FindPathToLocationSynchronously(
        CurrentWorld, StartLocation, DestinationLocation, const_cast<AActor*>(OwnerActor));

    if (CalculatedPath == nullptr || !CalculatedPath->IsValid() || CalculatedPath->PathPoints.Num() < 2)
    {
        mWaypoints.Empty();
        mCurrentWaypointIndex = 0;
        OnPathCalculated.Broadcast(false, 0);
        return false;
    }

    mWaypoints = CalculatedPath->PathPoints;
    mCurrentWaypointIndex = 1;

    OnPathCalculated.Broadcast(true, mWaypoints.Num());
    return true;
}

bool UVKNavPathPlannerComponent::GetCurrentWaypoint(FVector& OutWaypoint) const
{
    if (!mWaypoints.IsValidIndex(mCurrentWaypointIndex))
    {
        return false;
    }

    OutWaypoint = mWaypoints[mCurrentWaypointIndex];
    return true;
}

bool UVKNavPathPlannerComponent::AdvanceToNextWaypoint()
{
    if (mCurrentWaypointIndex + 1 < mWaypoints.Num())
    {
        mCurrentWaypointIndex++;
        return true;
    }

    return false;
}

bool UVKNavPathPlannerComponent::HasReachedDestination(float const AcceptanceRadius) const
{
    AActor const* OwnerActor = GetOwner();
    if (OwnerActor == nullptr)
    {
        return false;
    }

    FVector const CurrentLoc = OwnerActor->GetActorLocation();
    FVector const DestLoc = GetTargetDestination();
    float const Distance2D = FVector::Dist2D(CurrentLoc, DestLoc);

    return Distance2D <= AcceptanceRadius;
}

void UVKNavPathPlannerComponent::SetRepathInterval(float const NewInterval)
{
    mRepathInterval = FMath::Max(0.05f, NewInterval);
}

float UVKNavPathPlannerComponent::GetRepathInterval() const
{
    return mRepathInterval;
}

void UVKNavPathPlannerComponent::SetAcceptanceRadius(float const NewRadius)
{
    mAcceptanceRadius = FMath::Max(10.0f, NewRadius);
}

float UVKNavPathPlannerComponent::GetAcceptanceRadius() const
{
    return mAcceptanceRadius;
}

bool UVKNavPathPlannerComponent::HasValidPath() const
{
    return mbIsNavigating && mWaypoints.IsValidIndex(mCurrentWaypointIndex);
}

TArray<FVector> const& UVKNavPathPlannerComponent::GetWaypoints() const
{
    return mWaypoints;
}

void UVKNavPathPlannerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!mbIsNavigating)
    {
        return;
    }

    AActor const* OwnerActor = GetOwner();
    if (OwnerActor == nullptr)
    {
        return;
    }

    if (HasReachedDestination(mAcceptanceRadius))
    {
        StopNavigation();
        OnDestinationReached.Broadcast();
        return;
    }

    FVector CurrentWaypoint;
    if (GetCurrentWaypoint(CurrentWaypoint))
    {
        float const DistToWaypoint2D = FVector::Dist2D(OwnerActor->GetActorLocation(), CurrentWaypoint);
        if (DistToWaypoint2D <= 100.0f)
        {
            AdvanceToNextWaypoint();
        }
    }

    mRepathTimer += DeltaTime;
    if (mRepathTimer >= mRepathInterval)
    {
        mRepathTimer = 0.0f;
        RequestPathCalculation();
    }
}
