#include "VKActionPoolComponent.h"
#include "VKEmbeddingEncoderRunner.h"
#include "VKActionDataAsset.h"

UVKActionPoolComponent::UVKActionPoolComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UVKActionPoolComponent::BeginPlay()
{
    Super::BeginPlay();

    if (mActionDataAsset != nullptr)
    {
        LoadFromDataAsset(mActionDataAsset);
    }
    else if (mActionMap.IsEmpty())
    {
        InitializeDefaultActions();
    }
}

void UVKActionPoolComponent::LoadFromDataAsset(UVKActionDataAsset const* DataAsset)
{
    if (DataAsset == nullptr)
    {
        return;
    }

    mActionMap.Empty();
    mCommandMappings.Empty();

    for (FBotMovementActionDef const& ActionDef : DataAsset->GetActions())
    {
        RegisterAction(ActionDef);
    }

    mCommandMappings = DataAsset->GetCommandMappings();
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
    MoveDef.mbKeepUntilStop = true;
    MoveDef.mTriggerPhrases = { TEXT("앞으로 가"), TEXT("전진해"), TEXT("직진"), TEXT("가자"), TEXT("forward"), TEXT("go") };
    RegisterAction(MoveDef);

    FBotMovementActionDef DashDef;
    DashDef.mActionId = FName(TEXT("Dash"));
    DashDef.mDisplayName = TEXT("돌진 대시");
    DashDef.mDescription = TEXT("빠른 속도로 목표 방향으로 단거리 돌진");
    DashDef.mSpeedMultiplier = 2.0f;
    DashDef.mDefaultDuration = 1.0f;
    DashDef.mAdaptiveDuration = 1.0f;
    DashDef.mbIsUnlocked = true;
    DashDef.mbCanTurn = false;
    DashDef.mTriggerPhrases = { TEXT("대시해"), TEXT("돌진해"), TEXT("앞으로 뛰어"), TEXT("달려"), TEXT("dash") };
    RegisterAction(DashDef);

    FBotMovementActionDef EvadeDef;
    EvadeDef.mActionId = FName(TEXT("Evade"));
    EvadeDef.mDisplayName = TEXT("긴급 회피");
    EvadeDef.mDescription = TEXT("위험을 피해 빠르게 회피 기동");
    EvadeDef.mSpeedMultiplier = 2.5f;
    EvadeDef.mDefaultDuration = 0.6f;
    EvadeDef.mAdaptiveDuration = 0.6f;
    EvadeDef.mbIsUnlocked = true;
    EvadeDef.mbCanTurn = true;
    EvadeDef.mTriggerPhrases = { TEXT("긴급 회피"), TEXT("구르기"), TEXT("피해"), TEXT("닷지"), TEXT("evade") };
    RegisterAction(EvadeDef);

    FBotMovementActionDef FallBackDef;
    FallBackDef.mActionId = FName(TEXT("FallBack"));
    FallBackDef.mDisplayName = TEXT("경계 후퇴");
    FallBackDef.mDescription = TEXT("뒤로 조심스럽게 거리를 벌리며 후퇴");
    FallBackDef.mSpeedMultiplier = 0.8f;
    FallBackDef.mDefaultDuration = 1.0f;
    FallBackDef.mAdaptiveDuration = 1.0f;
    FallBackDef.mbIsUnlocked = true;
    FallBackDef.mbCanTurn = true;
    FallBackDef.mTriggerPhrases = { TEXT("후퇴해"), TEXT("물러서"), TEXT("거리 벌려"), TEXT("빠져"), TEXT("back off") };
    RegisterAction(FallBackDef);

    FBotMovementActionDef ApproachDef;
    ApproachDef.mActionId = FName(TEXT("ApproachEnemy"));
    ApproachDef.mDisplayName = TEXT("적에게 접근");
    ApproachDef.mDescription = TEXT("시야 내 최적 적을 찾아 내비게이션 경로로 접근");
    ApproachDef.mSpeedMultiplier = 1.0f;
    ApproachDef.mDefaultDuration = 5.0f;
    ApproachDef.mAdaptiveDuration = 5.0f;
    ApproachDef.mbIsUnlocked = true;
    ApproachDef.mbCanTurn = true;
    ApproachDef.mTriggerPhrases = { TEXT("적에게 접근해"), TEXT("적에게 다가가"), TEXT("적에게 가"), TEXT("접근해"), TEXT("다가가"), TEXT("approach") };
    RegisterAction(ApproachDef);

    FBotMovementActionDef StopDef;
    StopDef.mActionId = FName(TEXT("Stop"));
    StopDef.mDisplayName = TEXT("정지");
    StopDef.mDescription = TEXT("진행 중인 모든 이동을 즉시 중단하고 정지");
    StopDef.mSpeedMultiplier = 0.0f;
    StopDef.mDefaultDuration = 0.0f;
    StopDef.mAdaptiveDuration = 0.0f;
    StopDef.mbIsUnlocked = true;
    StopDef.mbCanTurn = false;
    StopDef.mTriggerPhrases = { TEXT("정지"), TEXT("멈춰"), TEXT("스톱"), TEXT("그만"), TEXT("서"), TEXT("stop"), TEXT("멍쳐"), TEXT("멍차") };
    RegisterAction(StopDef);

    mCommandMappings.Empty();

    FVoiceCommandMapping MoveForward;
    MoveForward.mCommandId = FName(TEXT("MoveForward"));
    MoveForward.mTargetActionId = FName(TEXT("Move"));
    MoveForward.mDirection = FVector(1.0f, 0.0f, 0.0f);
    MoveForward.mTriggerPhrases = { TEXT("앞으로 가"), TEXT("전진해"), TEXT("직진"), TEXT("가자"), TEXT("앞으로"), TEXT("forward"), TEXT("go") };
    mCommandMappings.Add(MoveForward);

    FVoiceCommandMapping MoveBackward;
    MoveBackward.mCommandId = FName(TEXT("MoveBackward"));
    MoveBackward.mTargetActionId = FName(TEXT("Move"));
    MoveBackward.mDirection = FVector(-1.0f, 0.0f, 0.0f);
    MoveBackward.mTriggerPhrases = { TEXT("뒤로"), TEXT("뒤로 가"), TEXT("후진"), TEXT("후진해"), TEXT("back"), TEXT("backward") };
    mCommandMappings.Add(MoveBackward);

    FVoiceCommandMapping MoveLeft;
    MoveLeft.mCommandId = FName(TEXT("MoveLeft"));
    MoveLeft.mTargetActionId = FName(TEXT("Move"));
    MoveLeft.mDirection = FVector(0.0f, -1.0f, 0.0f);
    MoveLeft.mTriggerPhrases = { TEXT("왼쪽"), TEXT("좌측"), TEXT("왼쪽으로"), TEXT("왼쪽으로 가"), TEXT("left") };
    mCommandMappings.Add(MoveLeft);

    FVoiceCommandMapping MoveRight;
    MoveRight.mCommandId = FName(TEXT("MoveRight"));
    MoveRight.mTargetActionId = FName(TEXT("Move"));
    MoveRight.mDirection = FVector(0.0f, 1.0f, 0.0f);
    MoveRight.mTriggerPhrases = { TEXT("오른쪽"), TEXT("우측"), TEXT("오른쪽으로"), TEXT("오른쪽으로 가"), TEXT("right") };
    mCommandMappings.Add(MoveRight);

    FVoiceCommandMapping DashForward;
    DashForward.mCommandId = FName(TEXT("DashForward"));
    DashForward.mTargetActionId = FName(TEXT("Dash"));
    DashForward.mDirection = FVector(1.0f, 0.0f, 0.0f);
    DashForward.mTriggerPhrases = { TEXT("대시해"), TEXT("돌진해"), TEXT("앞으로 뛰어"), TEXT("달려"), TEXT("dash") };
    mCommandMappings.Add(DashForward);

    FVoiceCommandMapping EvadeCmd;
    EvadeCmd.mCommandId = FName(TEXT("EvadeCmd"));
    EvadeCmd.mTargetActionId = FName(TEXT("Evade"));
    EvadeCmd.mDirection = FVector(1.0f, 0.0f, 0.0f);
    EvadeCmd.mTriggerPhrases = { TEXT("긴급 회피"), TEXT("구르기"), TEXT("피해"), TEXT("닷지"), TEXT("evade") };
    mCommandMappings.Add(EvadeCmd);

    FVoiceCommandMapping FallBackCmd;
    FallBackCmd.mCommandId = FName(TEXT("FallBackCmd"));
    FallBackCmd.mTargetActionId = FName(TEXT("FallBack"));
    FallBackCmd.mDirection = FVector(-1.0f, 0.0f, 0.0f);
    FallBackCmd.mTriggerPhrases = { TEXT("후퇴해"), TEXT("물러서"), TEXT("거리 벌려"), TEXT("빠져"), TEXT("back off") };
    mCommandMappings.Add(FallBackCmd);

    FVoiceCommandMapping ApproachCmd;
    ApproachCmd.mCommandId = FName(TEXT("ApproachCmd"));
    ApproachCmd.mTargetActionId = FName(TEXT("ApproachEnemy"));
    ApproachCmd.mDirection = FVector(1.0f, 0.0f, 0.0f);
    ApproachCmd.mTriggerPhrases = { TEXT("적에게 접근해"), TEXT("적에게 다가가"), TEXT("적에게 가"), TEXT("접근해"), TEXT("다가가"), TEXT("approach") };
    mCommandMappings.Add(ApproachCmd);

    FVoiceCommandMapping StopCmd;
    StopCmd.mCommandId = FName(TEXT("StopCmd"));
    StopCmd.mTargetActionId = FName(TEXT("Stop"));
    StopCmd.mDirection = FVector::ZeroVector;
    StopCmd.mTriggerPhrases = { TEXT("정지"), TEXT("멈춰"), TEXT("스톱"), TEXT("그만"), TEXT("서"), TEXT("stop"), TEXT("멍쳐"), TEXT("멍차") };
    mCommandMappings.Add(StopCmd);
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

bool UVKActionPoolComponent::RegisterCustomAction(FBotMovementActionDef const& NewAction, TArray<FString> const& TriggerPhrases)
{
    if (NewAction.mActionId.IsNone())
    {
        return false;
    }

    FBotMovementActionDef CustomAction = NewAction;
    CustomAction.mTriggerPhrases = TriggerPhrases;
    CustomAction.mbIsUnlocked = true;
    CustomAction.mProficiency = 0.5f;
    CustomAction.mSuccessfulUses = 0;
    CustomAction.mTargetSpeedMultiplier = (NewAction.mSpeedMultiplier > 0.0f) ? NewAction.mSpeedMultiplier : 1.0f;
    CustomAction.mTargetDuration = NewAction.mDefaultDuration;
    CustomAction.mbKeepUntilStop = NewAction.mbKeepUntilStop;

    return RegisterAction(CustomAction);
}

bool UVKActionPoolComponent::AddTriggerPhrase(FName const& ActionId, FString const& NewPhrase)
{
    if (ActionId.IsNone() || NewPhrase.IsEmpty())
    {
        return false;
    }

    FBotMovementActionDef* FoundDef = mActionMap.Find(ActionId);
    if (FoundDef == nullptr)
    {
        return false;
    }

    FoundDef->mTriggerPhrases.AddUnique(NewPhrase);
    return true;
}

bool UVKActionPoolComponent::RemoveTriggerPhrase(FName const& ActionId, FString const& Phrase)
{
    if (ActionId.IsNone() || Phrase.IsEmpty())
    {
        return false;
    }

    FBotMovementActionDef* FoundDef = mActionMap.Find(ActionId);
    if (FoundDef == nullptr)
    {
        return false;
    }

    int32 const RemovedCount = FoundDef->mTriggerPhrases.Remove(Phrase);
    return RemovedCount > 0;
}

TArray<FString> UVKActionPoolComponent::GetTriggerPhrases(FName const& ActionId) const
{
    if (ActionId.IsNone())
    {
        return TArray<FString>();
    }

    FBotMovementActionDef const* FoundDef = mActionMap.Find(ActionId);
    if (FoundDef == nullptr)
    {
        return TArray<FString>();
    }

    return FoundDef->mTriggerPhrases;
}

void UVKActionPoolComponent::CacheActionEmbeddings(FVKEmbeddingEncoderRunner* EncoderRunner)
{
    if (EncoderRunner == nullptr || !EncoderRunner->IsInitialized())
    {
        return;
    }

    mAnchorCache.Empty();

    for (TPair<FName, FBotMovementActionDef> const& Pair : mActionMap)
    {
        FName const ActionId = Pair.Key;
        FBotMovementActionDef const& Def = Pair.Value;

        FActionAnchorEntry AnchorEntry;
        AnchorEntry.mActionId = ActionId;

        for (FString const& Phrase : Def.mTriggerPhrases)
        {
            TArray<float> Embedding;
            if (EncoderRunner->GetSentenceEmbedding(Phrase, Embedding))
            {
                FActionPhraseEmbedding PhraseEmbed;
                PhraseEmbed.mPhrase = Phrase;
                PhraseEmbed.mVector = MoveTemp(Embedding);
                AnchorEntry.mPhraseEmbeddings.Add(MoveTemp(PhraseEmbed));
            }
        }

        mAnchorCache.Add(ActionId, MoveTemp(AnchorEntry));
    }

    mCommandAnchorCache.Empty();

    for (FVoiceCommandMapping const& Mapping : mCommandMappings)
    {
        FCommandAnchorEntry CmdAnchor;
        CmdAnchor.mCommandId = Mapping.mCommandId;
        CmdAnchor.mTargetActionId = Mapping.mTargetActionId;
        CmdAnchor.mDirection = Mapping.mDirection;

        for (FString const& Phrase : Mapping.mTriggerPhrases)
        {
            TArray<float> Embedding;
            if (EncoderRunner->GetSentenceEmbedding(Phrase, Embedding))
            {
                FActionPhraseEmbedding PhraseEmbed;
                PhraseEmbed.mPhrase = Phrase;
                PhraseEmbed.mVector = MoveTemp(Embedding);
                CmdAnchor.mPhraseEmbeddings.Add(MoveTemp(PhraseEmbed));
            }
        }

        mCommandAnchorCache.Add(Mapping.mCommandId, MoveTemp(CmdAnchor));
    }
}

bool UVKActionPoolComponent::FindBestMatchingCommand(
    TArray<float> const& QueryEmbedding, float const SimilarityThreshold,
    FName& OutActionId, FVector& OutDirection, float& OutSimilarity) const
{
    OutActionId = NAME_None;
    OutDirection = FVector(1.0f, 0.0f, 0.0f);
    OutSimilarity = -1.0f;

    if (QueryEmbedding.IsEmpty())
    {
        return false;
    }

    int32 const Dim = QueryEmbedding.Num();
    float BestSimilarity = -1.0f;
    FName BestAction = NAME_None;
    FVector BestDirection = FVector(1.0f, 0.0f, 0.0f);

    if (!mCommandAnchorCache.IsEmpty())
    {
        for (TPair<FName, FCommandAnchorEntry> const& Pair : mCommandAnchorCache)
        {
            FCommandAnchorEntry const& Anchor = Pair.Value;
            if (!IsActionUnlocked(Anchor.mTargetActionId))
            {
                continue;
            }

            for (FActionPhraseEmbedding const& PhraseEmbed : Anchor.mPhraseEmbeddings)
            {
                if (PhraseEmbed.mVector.Num() != Dim)
                {
                    continue;
                }

                float DotProduct = 0.0f;
                for (int32 i = 0; i < Dim; ++i)
                {
                    DotProduct += QueryEmbedding[i] * PhraseEmbed.mVector[i];
                }

                if (DotProduct > BestSimilarity)
                {
                    BestSimilarity = DotProduct;
                    BestAction = Anchor.mTargetActionId;
                    BestDirection = Anchor.mDirection;
                }
            }
        }
    }
    else
    {
        for (TPair<FName, FActionAnchorEntry> const& AnchorPair : mAnchorCache)
        {
            FName const ActionId = AnchorPair.Key;
            if (!IsActionUnlocked(ActionId))
            {
                continue;
            }

            FActionAnchorEntry const& Anchor = AnchorPair.Value;
            for (FActionPhraseEmbedding const& PhraseEmbed : Anchor.mPhraseEmbeddings)
            {
                if (PhraseEmbed.mVector.Num() != Dim)
                {
                    continue;
                }

                float DotProduct = 0.0f;
                for (int32 i = 0; i < Dim; ++i)
                {
                    DotProduct += QueryEmbedding[i] * PhraseEmbed.mVector[i];
                }

                if (DotProduct > BestSimilarity)
                {
                    BestSimilarity = DotProduct;
                    BestAction = ActionId;
                }
            }
        }
    }

    OutSimilarity = BestSimilarity;
    if (BestAction.IsNone() || BestSimilarity < SimilarityThreshold)
    {
        return false;
    }

    OutActionId = BestAction;
    OutDirection = BestDirection;
    return true;
}

bool UVKActionPoolComponent::FindBestMatchingAction(
    TArray<float> const& QueryEmbedding, float const SimilarityThreshold,
    FName& OutActionId, float& OutSimilarity) const
{
    FVector UnusedDirection;
    return FindBestMatchingCommand(QueryEmbedding, SimilarityThreshold, OutActionId, UnusedDirection, OutSimilarity);
}

TArray<FVoiceCommandMapping> const& UVKActionPoolComponent::GetCommandMappings() const
{
    return mCommandMappings;
}

bool UVKActionPoolComponent::EvaluateActionPerformance(
    FName const& ActionId,
    float& OutSpeed,
    float& OutDuration,
    bool& bOutKeepUntilStop,
    bool& bOutCriticalFail) const
{
    OutSpeed = 1.0f;
    OutDuration = 1.0f;
    bOutKeepUntilStop = false;
    bOutCriticalFail = false;

    FBotMovementActionDef const* FoundDef = mActionMap.Find(ActionId);
    if (FoundDef == nullptr)
    {
        return false;
    }

    float const CriticalFailChance = 0.05f * (1.0f - FoundDef->mProficiency);
    if (FMath::FRand() < CriticalFailChance)
    {
        bOutCriticalFail = true;
        OutSpeed = 0.0f;
        OutDuration = 0.4f;
        bOutKeepUntilStop = false;
        return true;
    }

    float const SkillRatio = FMath::Clamp(FoundDef->mProficiency, 0.4f, 1.0f);
    OutSpeed = FMath::Lerp(1.0f, FoundDef->mTargetSpeedMultiplier, SkillRatio);
    bOutKeepUntilStop = FoundDef->mbKeepUntilStop;

    if (FoundDef->mbKeepUntilStop)
    {
        OutDuration = 0.0f;
    }
    else
    {
        float const MinDuration = FMath::Min(0.5f, FoundDef->mTargetDuration);
        OutDuration = FMath::Lerp(MinDuration, FoundDef->mTargetDuration, SkillRatio);
    }

    return true;
}

void UVKActionPoolComponent::RecordActionSuccess(FName const& ActionId, TArray<float> const& UtteranceEmbedding)
{
    FBotMovementActionDef* FoundDef = mActionMap.Find(ActionId);
    if (FoundDef == nullptr)
    {
        return;
    }

    FoundDef->mSuccessfulUses++;
    FoundDef->mProficiency = FMath::Min(1.0f, FoundDef->mProficiency + 0.05f);

    if (UtteranceEmbedding.IsEmpty())
    {
        return;
    }

    FActionAnchorEntry* FoundAnchor = mAnchorCache.Find(ActionId);
    if (FoundAnchor == nullptr || FoundAnchor->mPhraseEmbeddings.IsEmpty())
    {
        return;
    }

    int32 const Dim = UtteranceEmbedding.Num();
    for (FActionPhraseEmbedding& PhraseEmbed : FoundAnchor->mPhraseEmbeddings)
    {
        if (PhraseEmbed.mVector.Num() != Dim)
        {
            continue;
        }

        float NormSq = 0.0f;
        for (int32 i = 0; i < Dim; ++i)
        {
            PhraseEmbed.mVector[i] = PhraseEmbed.mVector[i] * 0.9f + UtteranceEmbedding[i] * 0.1f;
            NormSq += PhraseEmbed.mVector[i] * PhraseEmbed.mVector[i];
        }

        float const InvNorm = (NormSq > 1e-12f) ? FMath::InvSqrt(NormSq) : 0.0f;
        for (int32 i = 0; i < Dim; ++i)
        {
            PhraseEmbed.mVector[i] *= InvNorm;
        }
    }
}


