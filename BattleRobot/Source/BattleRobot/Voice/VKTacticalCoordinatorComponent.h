#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VKTacticalCoordinatorComponent.generated.h"

class UVKTargetPerceiverComponent;
class UVKNavPathPlannerComponent;
class UVKActiveActionComponent;
class ABattleRobotCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTacticalApproachStarted, AActor*, TargetActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTacticalApproachCompleted, AActor*, TargetActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTacticalApproachFailed, FString const&, Reason);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class BATTLEROBOT_API UVKTacticalCoordinatorComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UVKTacticalCoordinatorComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintCallable, Category="Tactics")
    bool ExecuteApproachEnemy();

    UFUNCTION(BlueprintCallable, Category="Tactics")
    void StopTactics();

    UFUNCTION(BlueprintCallable, Category="Tactics")
    bool IsExecutingTactics() const;

    UFUNCTION(BlueprintCallable, Category="Tactics")
    AActor* GetCurrentTacticalTarget() const;

    UPROPERTY(BlueprintAssignable, Category="Tactics")
    FOnTacticalApproachStarted OnApproachStarted;

    UPROPERTY(BlueprintAssignable, Category="Tactics")
    FOnTacticalApproachCompleted OnApproachCompleted;

    UPROPERTY(BlueprintAssignable, Category="Tactics")
    FOnTacticalApproachFailed OnApproachFailed;

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void HandleDestinationReached();

    UFUNCTION()
    void HandlePathCalculated(bool const bSuccess, int32 const WaypointCount);

    void UpdateMovementTowardsWaypoint();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactics")
    TWeakObjectPtr<UVKTargetPerceiverComponent> mTargetPerceiver;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactics")
    TWeakObjectPtr<UVKNavPathPlannerComponent> mNavPathPlanner;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactics")
    TWeakObjectPtr<UVKActiveActionComponent> mActiveAction;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactics")
    TWeakObjectPtr<ABattleRobotCharacter> mOwnerCharacter;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tactics")
    bool mbIsApproaching = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics")
    float mApproachSpeedMultiplier = 1.0f;
};
