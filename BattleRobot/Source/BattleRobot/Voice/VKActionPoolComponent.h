#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VKActionPoolComponent.generated.h"

USTRUCT(BlueprintType)
struct FBotMovementActionDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action")
    FName mActionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action")
    FString mDisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action")
    FString mDescription;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action")
    float mSpeedMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action")
    float mDefaultDuration = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action")
    float mAdaptiveDuration = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action")
    bool mbIsUnlocked = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Action")
    bool mbCanTurn = true;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActionPoolChanged, FName const&, ActionId);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class BATTLEROBOT_API UVKActionPoolComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UVKActionPoolComponent();

    UFUNCTION(BlueprintCallable, Category="ActionPool")
    bool RegisterAction(FBotMovementActionDef const& NewAction);

    UFUNCTION(BlueprintCallable, Category="ActionPool")
    bool UnregisterAction(FName const& ActionId);

    UFUNCTION(BlueprintCallable, Category="ActionPool")
    bool UnlockAction(FName const& ActionId);

    UFUNCTION(BlueprintCallable, Category="ActionPool")
    bool LockAction(FName const& ActionId);

    UFUNCTION(BlueprintCallable, Category="ActionPool")
    bool IsActionUnlocked(FName const& ActionId) const;

    UFUNCTION(BlueprintCallable, Category="ActionPool")
    float AdjustActionDuration(FName const& ActionId, int32 DeltaTicks);

    UFUNCTION(BlueprintCallable, Category="ActionPool")
    bool GetAction(FName const& ActionId, FBotMovementActionDef& OutAction) const;

    UFUNCTION(BlueprintCallable, Category="ActionPool")
    TArray<FBotMovementActionDef> GetAllActions() const;

    UFUNCTION(BlueprintCallable, Category="ActionPool")
    FString BuildLLMActionCatalog() const;

    UFUNCTION(BlueprintCallable, Category="ActionPool")
    void ResetToDefaultActions();

    UPROPERTY(BlueprintAssignable, Category="ActionPool")
    FOnActionPoolChanged OnActionRegistered;

    UPROPERTY(BlueprintAssignable, Category="ActionPool")
    FOnActionPoolChanged OnActionUnregistered;

protected:
    virtual void BeginPlay() override;

    void InitializeDefaultActions();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ActionPool")
    TMap<FName, FBotMovementActionDef> mActionMap;
};
