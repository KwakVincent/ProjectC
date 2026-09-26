#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VKNavPathPlannerComponent.generated.h"

class UNavigationSystemV1;
class UNavigationPath;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPathCalculated, bool, bSuccess, int32, WaypointCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDestinationReached);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class BATTLEROBOT_API UVKNavPathPlannerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UVKNavPathPlannerComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintCallable, Category="Navigation")
    bool SetTargetActor(AActor* TargetActor);

    UFUNCTION(BlueprintCallable, Category="Navigation")
    bool SetTargetLocation(FVector const& TargetLocation);

    UFUNCTION(BlueprintCallable, Category="Navigation")
    void StopNavigation();

    UFUNCTION(BlueprintCallable, Category="Navigation")
    bool RequestPathCalculation();

    UFUNCTION(BlueprintCallable, Category="Navigation")
    bool GetCurrentWaypoint(FVector& OutWaypoint) const;

    UFUNCTION(BlueprintCallable, Category="Navigation")
    bool AdvanceToNextWaypoint();

    UFUNCTION(BlueprintCallable, Category="Navigation")
    bool HasReachedDestination(float const AcceptanceRadius) const;

    UFUNCTION(BlueprintCallable, Category="Navigation")
    void SetRepathInterval(float const NewInterval);

    UFUNCTION(BlueprintCallable, Category="Navigation")
    float GetRepathInterval() const;

    UFUNCTION(BlueprintCallable, Category="Navigation")
    void SetAcceptanceRadius(float const NewRadius);

    UFUNCTION(BlueprintCallable, Category="Navigation")
    float GetAcceptanceRadius() const;

    UFUNCTION(BlueprintCallable, Category="Navigation")
    bool HasValidPath() const;

    UFUNCTION(BlueprintCallable, Category="Navigation")
    TArray<FVector> const& GetWaypoints() const;

    UPROPERTY(BlueprintAssignable, Category="Navigation")
    FOnPathCalculated OnPathCalculated;

    UPROPERTY(BlueprintAssignable, Category="Navigation")
    FOnDestinationReached OnDestinationReached;

protected:
    virtual void BeginPlay() override;
    UNavigationSystemV1* GetNavigationSystem() const;
    FVector GetTargetDestination() const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Navigation")
    float mRepathInterval = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Navigation")
    float mAcceptanceRadius = 150.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Navigation")
    TWeakObjectPtr<AActor> mTargetActor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Navigation")
    FVector mTargetLocation = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Navigation")
    bool mbUseActorTarget = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Navigation")
    bool mbIsNavigating = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Navigation")
    TArray<FVector> mWaypoints;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Navigation")
    int32 mCurrentWaypointIndex = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Navigation")
    float mRepathTimer = 0.0f;
};
