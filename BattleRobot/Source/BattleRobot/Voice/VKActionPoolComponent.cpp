#include "VKActionPoolComponent.h"

UVKActionPoolComponent::UVKActionPoolComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UVKActionPoolComponent::BeginPlay()
{
    Super::BeginPlay();

    if (mActionMap.IsEmpty())
    {
        InitializeDefaultActions();
    }
}

void UVKActionPoolComponent::InitializeDefaultActions()
{
    mActionMap.Empty();

    FBotMovementActionDef MoveDef;
    MoveDef.mActionId = FName(TEXT("Move"));
    MoveDef.mDisplayName = TEXT("일반 이동");
    MoveDef.mDescription = TEXT("전진, 후진, 좌우 방향으로 표준 속도로 이동");
    MoveDef.mSpeedMultiplier = 1.0f;
    MoveDef.mDefaultDuration = 1.0f;
    MoveDef.mAdaptiveDuration = 1.0f;
    MoveDef.mbIsUnlocked = true;
    MoveDef.mbCanTurn = true;
    RegisterAction(MoveDef);

    FBotMovementActionDef DashDef;
    DashDef.mActionId = FName(TEXT("Dash"));
    DashDef.mDisplayName = TEXT("돌진 대시");
    DashDef.mDescription = TEXT("빠른 속도로 목표 방향으로 단거리 돌진");
    DashDef.mSpeedMultiplier = 2.0f;
    DashDef.mDefaultDuration = 1.0f;
    DashDef.mAdaptiveDuration = 1.0f;
    DashDef.mbIsUnlocked = false;
    DashDef.mbCanTurn = false;
    RegisterAction(DashDef);

    FBotMovementActionDef EvadeDef;
    EvadeDef.mActionId = FName(TEXT("Evade"));
    EvadeDef.mDisplayName = TEXT("긴급 회피");
    EvadeDef.mDescription = TEXT("위험을 피해 빠르게 회피 기동");
    EvadeDef.mSpeedMultiplier = 2.5f;
    EvadeDef.mDefaultDuration = 0.6f;
    EvadeDef.mAdaptiveDuration = 0.6f;
    EvadeDef.mbIsUnlocked = false;
    EvadeDef.mbCanTurn = true;
    RegisterAction(EvadeDef);

    FBotMovementActionDef FallBackDef;
    FallBackDef.mActionId = FName(TEXT("FallBack"));
    FallBackDef.mDisplayName = TEXT("경계 후퇴");
    FallBackDef.mDescription = TEXT("뒤로 조심스럽게 거리를 벌리며 후퇴");
    FallBackDef.mSpeedMultiplier = 0.8f;
    FallBackDef.mDefaultDuration = 1.0f;
    FallBackDef.mAdaptiveDuration = 1.0f;
    FallBackDef.mbIsUnlocked = false;
    FallBackDef.mbCanTurn = true;
    RegisterAction(FallBackDef);

    FBotMovementActionDef StopDef;
    StopDef.mActionId = FName(TEXT("Stop"));
    StopDef.mDisplayName = TEXT("정지");
    StopDef.mDescription = TEXT("진행 중인 모든 이동을 즉시 중단하고 정지");
    StopDef.mSpeedMultiplier = 0.0f;
    StopDef.mDefaultDuration = 0.0f;
    StopDef.mAdaptiveDuration = 0.0f;
    StopDef.mbIsUnlocked = true;
    StopDef.mbCanTurn = false;
    RegisterAction(StopDef);
}

void UVKActionPoolComponent::ResetToDefaultActions()
{
    InitializeDefaultActions();
}

bool UVKActionPoolComponent::RegisterAction(FBotMovementActionDef const& NewAction)
{
    if (NewAction.mActionId.IsNone())
    {
        return false;
    }

    mActionMap.Add(NewAction.mActionId, NewAction);
    OnActionRegistered.Broadcast(NewAction.mActionId);
    return true;
}

bool UVKActionPoolComponent::UnregisterAction(FName const& ActionId)
{
    if (ActionId.IsNone() || !mActionMap.Contains(ActionId))
    {
        return false;
    }

    mActionMap.Remove(ActionId);
    OnActionUnregistered.Broadcast(ActionId);
    return true;
}

bool UVKActionPoolComponent::UnlockAction(FName const& ActionId)
{
    if (ActionId.IsNone())
    {
        return false;
    }

    FBotMovementActionDef* FoundDef = mActionMap.Find(ActionId);
    if (FoundDef == nullptr)
    {
        return false;
    }

    FoundDef->mbIsUnlocked = true;
    return true;
}

bool UVKActionPoolComponent::LockAction(FName const& ActionId)
{
    if (ActionId.IsNone())
    {
        return false;
    }

    FBotMovementActionDef* FoundDef = mActionMap.Find(ActionId);
    if (FoundDef == nullptr)
    {
        return false;
    }

    FoundDef->mbIsUnlocked = false;
    return true;
}

bool UVKActionPoolComponent::IsActionUnlocked(FName const& ActionId) const
{
    if (ActionId.IsNone())
    {
        return false;
    }

    FBotMovementActionDef const* FoundDef = mActionMap.Find(ActionId);
    if (FoundDef == nullptr)
    {
        return false;
    }

    return FoundDef->mbIsUnlocked;
}

float UVKActionPoolComponent::AdjustActionDuration(FName const& ActionId, int32 DeltaTicks)
{
    if (ActionId.IsNone() || DeltaTicks == 0)
    {
        return -1.0f;
    }

    FBotMovementActionDef* FoundDef = mActionMap.Find(ActionId);
    if (FoundDef == nullptr)
    {
        return -1.0f;
    }

    float const DeltaSeconds = static_cast<float>(DeltaTicks) * 0.1f;
    float const NewDuration = FMath::Clamp(FoundDef->mAdaptiveDuration + DeltaSeconds, 0.2f, 10.0f);
    FoundDef->mAdaptiveDuration = NewDuration;
    return NewDuration;
}

bool UVKActionPoolComponent::GetAction(FName const& ActionId, FBotMovementActionDef& OutAction) const
{
    if (ActionId.IsNone())
    {
        return false;
    }

    FBotMovementActionDef const* FoundDef = mActionMap.Find(ActionId);
    if (FoundDef == nullptr)
    {
        return false;
    }

    OutAction = *FoundDef;
    return true;
}

TArray<FBotMovementActionDef> UVKActionPoolComponent::GetAllActions() const
{
    TArray<FBotMovementActionDef> ResultList;
    mActionMap.GenerateValueArray(ResultList);
    return ResultList;
}

FString UVKActionPoolComponent::BuildLLMActionCatalog() const
{
    if (mActionMap.IsEmpty())
    {
        return FString();
    }

    FString CatalogText;
    for (TPair<FName, FBotMovementActionDef> const& Pair : mActionMap)
    {
        FBotMovementActionDef const& Def = Pair.Value;
        if (!Def.mbIsUnlocked)
        {
            continue;
        }

        CatalogText += FString::Printf(TEXT("- %s: %s (현재학습지속시간: %.1f초, 속도배율: %.1f)\n"),
            *Def.mActionId.ToString(), *Def.mDescription, Def.mAdaptiveDuration, Def.mSpeedMultiplier);
    }

    return CatalogText;
}
