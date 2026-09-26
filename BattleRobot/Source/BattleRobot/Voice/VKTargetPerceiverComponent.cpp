#include "VKTargetPerceiverComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "CollisionQueryParams.h"
#include "Variant_Combat/Interfaces/CombatDamageable.h"

UVKTargetPerceiverComponent::UVKTargetPerceiverComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

APlayerController* UVKTargetPerceiverComponent::GetPlayerController() const
{
    AActor const* OwnerActor = GetOwner();
    if (OwnerActor == nullptr)
    {
        return nullptr;
    }

    APawn const* OwnerPawn = Cast<APawn>(OwnerActor);
    if (OwnerPawn != nullptr)
    {
        return Cast<APlayerController>(OwnerPawn->GetController());
    }

    UWorld* CurrentWorld = GetWorld();
    if (CurrentWorld == nullptr)
    {
        return nullptr;
    }

    return UGameplayStatics::GetPlayerController(CurrentWorld, 0);
}

bool UVKTargetPerceiverComponent::IsEnemyActor(AActor const* CandidateActor) const
{
    if (CandidateActor == nullptr || CandidateActor == GetOwner())
    {
        return false;
    }

    if (CandidateActor->ActorHasTag(mEnemyTag))
    {
        return true;
    }

    if (CandidateActor->GetClass()->ImplementsInterface(UCombatDamageable::StaticClass()))
    {
        return true;
    }

    return false;
}

bool UVKTargetPerceiverComponent::CheckLineOfSight(FVector const& ViewLocation, AActor const* TargetActor) const
{
    if (TargetActor == nullptr)
    {
        return false;
    }

    UWorld* CurrentWorld = GetWorld();
    if (CurrentWorld == nullptr)
    {
        return false;
    }

    FVector const TargetLocation = TargetActor->GetActorLocation();
    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(GetOwner());
    QueryParams.AddIgnoredActor(TargetActor);

    bool const bHit = CurrentWorld->LineTraceSingleByChannel(
        HitResult, ViewLocation, TargetLocation, ECC_Visibility, QueryParams);

    return !bHit;
}

TArray<FTargetCandidate> UVKTargetPerceiverComponent::FindEnemiesInView()
{
    TArray<FTargetCandidate> Candidates;

    APlayerController* PC = GetPlayerController();
    if (PC == nullptr || GetWorld() == nullptr)
    {
        return Candidates;
    }

    int32 ViewportSizeX = 0;
    int32 ViewportSizeY = 0;
    PC->GetViewportSize(ViewportSizeX, ViewportSizeY);
    if (ViewportSizeX <= 0 || ViewportSizeY <= 0)
    {
        return Candidates;
    }

    FVector2D const ScreenCenter(ViewportSizeX * 0.5f, ViewportSizeY * 0.5f);
    float const MaxScreenDistance = ScreenCenter.Size();

    FVector CameraLocation = FVector::ZeroVector;
    FRotator CameraRotation = FRotator::ZeroRotator;
    PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

    AActor const* OwnerActor = GetOwner();
    FVector const OwnerLocation = (OwnerActor != nullptr) ? OwnerActor->GetActorLocation() : CameraLocation;

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsWithInterface(GetWorld(), UCombatDamageable::StaticClass(), AllActors);

    TArray<AActor*> TaggedActors;
    UGameplayStatics::GetAllActorsWithTag(GetWorld(), mEnemyTag, TaggedActors);
    for (AActor* TaggedActor : TaggedActors)
    {
        AllActors.AddUnique(TaggedActor);
    }

    for (AActor* CandidateActor : AllActors)
    {
        if (!IsEnemyActor(CandidateActor))
        {
            continue;
        }

        FVector const EnemyLocation = CandidateActor->GetActorLocation();
        float const WorldDist = FVector::Dist(OwnerLocation, EnemyLocation);
        if (WorldDist > mMaxPerceptionDistance)
        {
            continue;
        }

        FVector2D ScreenPos;
        bool const bProjected = PC->ProjectWorldLocationToScreen(EnemyLocation, ScreenPos, true);
        if (!bProjected)
        {
            continue;
        }

        bool const bInScreenBounds = (ScreenPos.X >= 0.0f && ScreenPos.X <= ViewportSizeX &&
                                      ScreenPos.Y >= 0.0f && ScreenPos.Y <= ViewportSizeY);
        if (!bInScreenBounds)
        {
            continue;
        }

        if (!CheckLineOfSight(CameraLocation, CandidateActor))
        {
            continue;
        }

        float const PixelDistToCenter = FVector2D::Distance(ScreenPos, ScreenCenter);
        float const NormScreenDist = (MaxScreenDistance > 0.0f) ? (PixelDistToCenter / MaxScreenDistance) : 0.0f;
        float const NormWorldDist = (mMaxPerceptionDistance > 0.0f) ? (WorldDist / mMaxPerceptionDistance) : 0.0f;
        float const Score = (NormScreenDist * mScreenCenterWeight) + (NormWorldDist * mWorldDistanceWeight);

        FTargetCandidate Candidate;
        Candidate.TargetActor = CandidateActor;
        Candidate.ScreenPosition = ScreenPos;
        Candidate.DistanceToCenter = PixelDistToCenter;
        Candidate.WorldDistance = WorldDist;
        Candidate.Score = Score;

        Candidates.Add(Candidate);
    }

    return Candidates;
}

AActor* UVKTargetPerceiverComponent::AcquireBestTarget()
{
    TArray<FTargetCandidate> Candidates = FindEnemiesInView();
    if (Candidates.IsEmpty())
    {
        ClearTarget();
        return nullptr;
    }

    Candidates.Sort([](FTargetCandidate const& A, FTargetCandidate const& B)
    {
        return A.Score < B.Score;
    });

    AActor* BestTarget = Candidates[0].TargetActor.Get();
    if (BestTarget == nullptr)
    {
        ClearTarget();
        return nullptr;
    }

    mCurrentTarget = BestTarget;
    OnTargetAcquired.Broadcast(BestTarget);
    return BestTarget;
}

AActor* UVKTargetPerceiverComponent::GetCurrentTarget() const
{
    return mCurrentTarget.Get();
}

void UVKTargetPerceiverComponent::ClearTarget()
{
    if (mCurrentTarget.IsValid())
    {
        mCurrentTarget.Reset();
        OnTargetLost.Broadcast();
    }
}
