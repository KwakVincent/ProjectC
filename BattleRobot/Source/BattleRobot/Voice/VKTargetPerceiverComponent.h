#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VKTargetPerceiverComponent.generated.h"

class APlayerController;

USTRUCT(BlueprintType)
struct FTargetCandidate
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Target")
    TWeakObjectPtr<AActor> TargetActor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Target")
    FVector2D ScreenPosition = FVector2D::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Target")
    float DistanceToCenter = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Target")
    float WorldDistance = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Target")
    float Score = 0.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTargetAcquired, AActor*, TargetActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTargetLost);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class BATTLEROBOT_API UVKTargetPerceiverComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UVKTargetPerceiverComponent();

    UFUNCTION(BlueprintCallable, Category="Perceiver")
    AActor* AcquireBestTarget();

    UFUNCTION(BlueprintCallable, Category="Perceiver")
    TArray<FTargetCandidate> FindEnemiesInView();

    UFUNCTION(BlueprintCallable, Category="Perceiver")
    AActor* GetCurrentTarget() const;

    UFUNCTION(BlueprintCallable, Category="Perceiver")
    void ClearTarget();

    UPROPERTY(BlueprintAssignable, Category="Perceiver")
    FOnTargetAcquired OnTargetAcquired;

    UPROPERTY(BlueprintAssignable, Category="Perceiver")
    FOnTargetLost OnTargetLost;

protected:
    APlayerController* GetPlayerController() const;
    bool IsEnemyActor(AActor const* CandidateActor) const;
    bool CheckLineOfSight(FVector const& ViewLocation, AActor const* TargetActor) const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Perceiver")
    FName mEnemyTag = TEXT("Enemy");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Perceiver")
    float mMaxPerceptionDistance = 5000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Perceiver")
    float mScreenCenterWeight = 0.7f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Perceiver")
    float mWorldDistanceWeight = 0.3f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Perceiver")
    TWeakObjectPtr<AActor> mCurrentTarget;
};
