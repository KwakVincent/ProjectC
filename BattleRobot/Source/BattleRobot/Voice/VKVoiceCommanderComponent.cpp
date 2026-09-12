#include "VKVoiceCommanderComponent.h"
#include "VoicePipeline.h"
#include "BattleRobotCharacter.h"
#include "VKActionPoolComponent.h"
#include "VKActiveActionComponent.h"
#include "VKActionSelector.h"
#include "VKEmbeddingEncoderRunner.h"
#include "VKActionSpeechLearner.h"
#include "Misc/Paths.h"
#include "Async/Async.h"

UVKVoiceCommanderComponent::UVKVoiceCommanderComponent()
    : mMoveDuration(2.0f)
    , mMaxConsecutiveActions(1)
    , mSimilarityThreshold(0.65f)
    , mActionPool(nullptr)
    , mActiveAction(nullptr)
    , mEncoderRunner(nullptr)
    , mActiveCommand(EBotVoiceCommand::None)
    , mRemainingMoveTime(0.0f)
    , mbIsTrainingMode(false)
    , mbIsContinuousMoving(false)
    , mContinuousDirection(FVector::ZeroVector)
    , mContinuousSpeedMultiplier(1.0f)
{
    PrimaryComponentTick.bCanEverTick = true;
}

UVKVoiceCommanderComponent::~UVKVoiceCommanderComponent() = default;

void UVKVoiceCommanderComponent::SetSimilarityThreshold(float const NewThreshold)
{
    mSimilarityThreshold = FMath::Clamp(NewThreshold, 0.0f, 1.0f);
    if (GConfig != nullptr)
    {
        GConfig->SetFloat(TEXT("VoiceProfile"), TEXT("SimilarityThreshold"), mSimilarityThreshold, GGameIni);
        GConfig->Flush(false, GGameIni);
    }
    UE_LOG(LogTemp, Log, TEXT(">> [음성 유사도 임계치 설정]: %.2f (Game.ini 저장 완료)"), mSimilarityThreshold);
}

float UVKVoiceCommanderComponent::GetSimilarityThreshold() const
{
    return mSimilarityThreshold;
}

void UVKVoiceCommanderComponent::SetTrainingMode(bool const bEnable)
{
    mbIsTrainingMode = bEnable;
    UE_LOG(LogTemp, Log, TEXT(">> [음성 모드 전환]: %s"), bEnable ? TEXT("훈련 모드 (새 액션 학습 가능)") : TEXT("사용 모드 (실전 명령)"));
}

bool UVKVoiceCommanderComponent::IsTrainingMode() const
{
    return mbIsTrainingMode;
}

UVKActionPoolComponent* UVKVoiceCommanderComponent::GetActionPool() const
{
    return mActionPool;
}

void UVKVoiceCommanderComponent::SetConsecutiveActionLevel(int32 const NewLevel)
{
    mMaxConsecutiveActions = FMath::Clamp(NewLevel, 1, 10);
    UE_LOG(LogTemp, Log, TEXT(">> [연속 명령 단계 설정]: 현재 %d단계 해금 (최대 %d개 연속 액션 실행 가능)"),
        mMaxConsecutiveActions, mMaxConsecutiveActions);
}

int32 UVKVoiceCommanderComponent::GetConsecutiveActionLevel() const
{
    return mMaxConsecutiveActions;
}

UVKActiveActionComponent* UVKVoiceCommanderComponent::GetActiveAction() const
{
    return mActiveAction;
}

FVKEmbeddingEncoderRunner* UVKVoiceCommanderComponent::GetEncoderRunner() const
{
    return mEncoderRunner.Get();
}

void UVKVoiceCommanderComponent::BeginPlay()
{
    Super::BeginPlay();

    AActor* OwnerActor = GetOwner();
    if (OwnerActor == nullptr)
    {
        return;
    }

    mOwnerCharacter = Cast<ABattleRobotCharacter>(OwnerActor);
    mClassifier = MakeUnique<FRuleBasedCommandClassifier>();

    mActionPool = OwnerActor->FindComponentByClass<UVKActionPoolComponent>();
    if (mActionPool == nullptr)
    {
        mActionPool = NewObject<UVKActionPoolComponent>(OwnerActor, TEXT("ActionPool"));
        mActionPool->RegisterComponent();
    }

    mActiveAction = OwnerActor->FindComponentByClass<UVKActiveActionComponent>();
    if (mActiveAction == nullptr)
    {
        mActiveAction = NewObject<UVKActiveActionComponent>(OwnerActor, TEXT("ActiveAction"));
        mActiveAction->RegisterComponent();
    }

    mEncoderRunner = MakeUnique<FVKEmbeddingEncoderRunner>();
    FString const ModelPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Models/ko_sroberta_onnx"));
    Async(EAsyncExecution::Thread, [this, ModelPath]()
    {
        if (mEncoderRunner)
        {
            if (mEncoderRunner->Initialize(ModelPath))
            {
                if (mActionPool)
                {
                    mActionPool->CacheActionEmbeddings(mEncoderRunner.Get());
                    UE_LOG(LogTemp, Log, TEXT(">> [온디바이스 인코더 모델 초기화 및 액션 임베딩 캐싱 완료]"));
                }
            }
        }
    });

    StartVoiceRecognition();
}

void UVKVoiceCommanderComponent::EndPlay(EEndPlayReason::Type const EndPlayReason)
{
    StopVoiceRecognition();

    if (mEncoderRunner)
    {
        mEncoderRunner->Shutdown();
        mEncoderRunner.Reset();
    }

    Super::EndPlay(EndPlayReason);
}

void UVKVoiceCommanderComponent::StartVoiceRecognition()
{
    if (mVoicePipeline)
    {
        return;
    }

    int32 SavedSilenceLimit = FVoicePipeline::DEFAULT_SILENCE_CHUNKS_LIMIT;
    if (GConfig != nullptr)
    {
        GConfig->GetInt(TEXT("VoiceProfile"), TEXT("UserSilenceLimit"), SavedSilenceLimit, GGameIni);
        GConfig->GetFloat(TEXT("VoiceProfile"), TEXT("SimilarityThreshold"), mSimilarityThreshold, GGameIni);
    }

    FString const VadPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Models/silero_vad.onnx"));
    FString const WhisperPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Models/ggml-tiny.bin"));

    mVoicePipeline = MakeUnique<FVoicePipeline>(VadPath, WhisperPath, SavedSilenceLimit);
    UE_LOG(LogTemp, Log, TEXT(">> [음성 파이프라인 시작] 저장된 VAD 침묵 한계: %d 청크 (약 %d ms), 유사도 임계치: %.2f"),
        SavedSilenceLimit, SavedSilenceLimit * 32, mSimilarityThreshold);

    mVoicePipeline->SetOnSpeechRecognized([this](FString const& Text, double VadMs, double SttMs)
    {
        HandleSpeechRecognized(Text, VadMs, SttMs);
    });

    mVoicePipeline->StartCapture();
}

void UVKVoiceCommanderComponent::StopVoiceRecognition()
{
    if (!mVoicePipeline)
    {
        return;
    }

    mVoicePipeline->StopCapture();
    mVoicePipeline.Reset();
}

void UVKVoiceCommanderComponent::RestartVoiceRecognition()
{
    StopVoiceRecognition();
    StartVoiceRecognition();
}

bool UVKVoiceCommanderComponent::IsMicrophoneCapturing() const
{
    if (mVoicePipeline)
    {
        return mVoicePipeline->IsCapturing();
    }
    return false;
}

void UVKVoiceCommanderComponent::HandleSpeechRecognized(FString const& RecognizedText, double VadDurationMs, double SttDurationMs)
{
    if (RecognizedText.IsEmpty())
    {
        return;
    }

    if (mbIsTrainingMode)
    {
        Async(EAsyncExecution::Thread, [this, RecognizedText]()
        {
            FBotMovementActionDef NewActionDef;
            TArray<FString> TriggerPhrases;
            if (FVKActionSpeechLearner::ParseActionFromSpeech(RecognizedText, NewActionDef, TriggerPhrases))
            {
                AsyncTask(ENamedThreads::GameThread, [this, NewActionDef, TriggerPhrases]()
                {
                    if (mActionPool)
                    {
                        mActionPool->RegisterCustomAction(NewActionDef, TriggerPhrases);
                        mActionPool->CacheActionEmbeddings(mEncoderRunner.Get());
                        UE_LOG(LogTemp, Log, TEXT(">> [음성 훈련 성공]: 액션 '%s' 등록! (속도: %.1f배, 시간: %.1f초, 무한지속: %d)"),
                            *NewActionDef.mActionId.ToString(), NewActionDef.mSpeedMultiplier, NewActionDef.mDefaultDuration, NewActionDef.mbKeepUntilStop);
                        OnActionTrained.Broadcast(NewActionDef.mActionId, NewActionDef.mSpeedMultiplier, NewActionDef.mDefaultDuration, NewActionDef.mbKeepUntilStop);
                    }
                });
            }
        });
        return;
    }

    Async(EAsyncExecution::Thread, [this, RecognizedText, VadDurationMs, SttDurationMs]()
    {
        double const InferStart = FPlatformTime::Seconds();

        int32 const AllowedActions = mMaxConsecutiveActions;
        TArray<FBotActionParseResult> const ParseResults = UVKActionSelector::ClassifyVoiceCommandSequence(
            RecognizedText, mActionPool, mEncoderRunner.Get(), AllowedActions, mSimilarityThreshold);

        double const InferDurationMs = (FPlatformTime::Seconds() - InferStart) * 1000.0;
        double const DispatchStartTime = FPlatformTime::Seconds();

        AsyncTask(ENamedThreads::GameThread, [this, ParseResults, RecognizedText, VadDurationMs, SttDurationMs, InferDurationMs, DispatchStartTime]()
        {
            ExecuteActionSequenceOnGameThread(ParseResults, RecognizedText, VadDurationMs, SttDurationMs, InferDurationMs, DispatchStartTime);
        });
    });
}

void UVKVoiceCommanderComponent::HandleCommandUnrecognized(FString const& RawText)
{
    UE_LOG(LogTemp, Warning, TEXT(">> [음성 명령 미인식(실패)]: '%s' (일치하는 액션을 찾을 수 없어 명령을 실행하지 않습니다. UI/피드백 브로드캐스트)"), *RawText);
    OnVoiceCommandUnrecognized.Broadcast(RawText);
}

void UVKVoiceCommanderComponent::ExecuteActionOnGameThread(
    FName const& ActionId,
    FVector const& Direction,
    int32 DeltaTicks,
    FString const& RawText,
    double VadDurationMs,
    double SttDurationMs,
    double InferDurationMs,
    double DispatchStartTime)
{
    if (!IsValid(this) || mActiveAction == nullptr)
    {
        return;
    }

    double const ActionDispatchMs = (FPlatformTime::Seconds() - DispatchStartTime) * 1000.0;
    double const TotalLatencyFromSpeechEnd = SttDurationMs + InferDurationMs + ActionDispatchMs;

    float ActualSpeed = 1.0f;
    float ActualDuration = mMoveDuration;
    bool bKeepUntilStop = false;
    bool bCriticalFail = false;

    if (mActionPool != nullptr)
    {
        if (DeltaTicks != 0)
        {
            float const UpdatedDuration = mActionPool->AdjustActionDuration(ActionId, DeltaTicks);
            UE_LOG(LogTemp, Log, TEXT(">> [적응형 지속시간 조절]: 액션 %s, Tick: %+d, 지속시간: %.2f초"),
                *ActionId.ToString(), DeltaTicks, UpdatedDuration);
        }

        if (mActionPool->EvaluateActionPerformance(ActionId, ActualSpeed, ActualDuration, bKeepUntilStop, bCriticalFail))
        {
            if (bCriticalFail)
            {
                UE_LOG(LogTemp, Warning, TEXT(">> [로봇 주춤]: 액션 '%s' 숙련도 부족으로 동작을 수행하지 못했습니다!"), *ActionId.ToString());
                OnActionExecutionImperfect.Broadcast(ActionId);
                return;
            }
        }
    }

    if (ActionId == FName(TEXT("Stop")))
    {
        mbIsContinuousMoving = false;
        mActiveAction->ApplyAction(ActionId, FVector::ZeroVector, 0.0f, 0.0f);
    }
    else if (bKeepUntilStop)
    {
        mbIsContinuousMoving = true;
        mContinuousDirection = Direction;
        mContinuousSpeedMultiplier = ActualSpeed;
        mActiveAction->ApplyAction(ActionId, Direction, ActualSpeed, 0.0f);
    }
    else
    {
        mbIsContinuousMoving = false;
        mActiveAction->ApplyAction(ActionId, Direction, ActualSpeed, ActualDuration);
    }

    if (mActionPool != nullptr && mEncoderRunner && mEncoderRunner->IsInitialized())
    {
        TArray<float> UtteranceEmbedding;
        if (mEncoderRunner->GetSentenceEmbedding(RawText, UtteranceEmbedding))
        {
            mActionPool->RecordActionSuccess(ActionId, UtteranceEmbedding);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("========================================================================="));
    UE_LOG(LogTemp, Log, TEXT(">> [음성 파이프라인 지연 시간 프로파일링 리포트]"));
    UE_LOG(LogTemp, Log, TEXT(" - 1. VAD 음성 활동 감지   : %.1f ms"), VadDurationMs);
    UE_LOG(LogTemp, Log, TEXT(" - 2. STT 텍스트 디코딩    : %.1f ms (원문: '%s')"), SttDurationMs, *RawText);
    UE_LOG(LogTemp, Log, TEXT(" - 3. 온디바이스 인코더 추론 : %.1f ms (결과: %s, 속도: %.1f배, 시간: %.2f초)"),
        InferDurationMs, *ActionId.ToString(), ActualSpeed, ActualDuration);
    UE_LOG(LogTemp, Log, TEXT(" - 4. 액션 게임스레드 실행 : %.1f ms"), ActionDispatchMs);
    UE_LOG(LogTemp, Log, TEXT(" ★ [체감 반응 지연 (발화 종료 -> 캐릭터 반응)]: %.1f ms"), TotalLatencyFromSpeechEnd);
    UE_LOG(LogTemp, Log, TEXT("========================================================================="));

    EBotVoiceCommand LegacyCommand = EBotVoiceCommand::None;
    if (ActionId == FName(TEXT("Stop")))
    {
        LegacyCommand = EBotVoiceCommand::Stop;
    }
    else if (Direction.X > 0.5f)
    {
        LegacyCommand = EBotVoiceCommand::Forward;
    }
    else if (Direction.X < -0.5f)
    {
        LegacyCommand = EBotVoiceCommand::Backward;
    }
    else if (Direction.Y < -0.5f)
    {
        LegacyCommand = EBotVoiceCommand::Left;
    }
    else if (Direction.Y > 0.5f)
    {
        LegacyCommand = EBotVoiceCommand::Right;
    }

    ExecuteCommandOnGameThread(LegacyCommand, RawText);
}

void UVKVoiceCommanderComponent::ExecuteActionSequenceOnGameThread(
    TArray<FBotActionParseResult> const& ParseResults,
    FString const& RawText,
    double VadDurationMs,
    double SttDurationMs,
    double InferDurationMs,
    double DispatchStartTime)
{
    if (!IsValid(this) || mActiveAction == nullptr)
    {
        return;
    }

    if (mVoicePipeline)
    {
        int32 const ValidCount = ParseResults.Num();
        mVoicePipeline->OnActionEvaluationFeedback(ValidCount);

        if (GConfig != nullptr && ValidCount >= 1)
        {
            int32 const UpdatedLimit = mVoicePipeline->GetSilenceLimit();
            GConfig->SetInt(TEXT("VoiceProfile"), TEXT("UserSilenceLimit"), UpdatedLimit, GGameIni);
            GConfig->Flush(false, GGameIni);
        }
    }

    if (ParseResults.IsEmpty())
    {
        HandleCommandUnrecognized(RawText);
        return;
    }

    if (ParseResults.Num() == 1)
    {
        FBotActionParseResult const& First = ParseResults[0];
        ExecuteActionOnGameThread(
            First.mActionId, First.mDirection, First.mDeltaTicks,
            RawText, VadDurationMs, SttDurationMs, InferDurationMs, DispatchStartTime);
        return;
    }

    double const ActionDispatchMs = (FPlatformTime::Seconds() - DispatchStartTime) * 1000.0;
    double const TotalLatencyFromSpeechEnd = SttDurationMs + InferDurationMs + ActionDispatchMs;

    TArray<FBotActionCommand> ActionSequence;
    for (FBotActionParseResult const& SubAction : ParseResults)
    {
        float ActualSpeed = 1.0f;
        float ActualDuration = mMoveDuration;
        bool bKeepUntilStop = false;
        bool bCriticalFail = false;

        if (mActionPool != nullptr)
        {
            if (SubAction.mDeltaTicks != 0)
            {
                mActionPool->AdjustActionDuration(SubAction.mActionId, SubAction.mDeltaTicks);
            }

            mActionPool->EvaluateActionPerformance(SubAction.mActionId, ActualSpeed, ActualDuration, bKeepUntilStop, bCriticalFail);
        }

        if (bCriticalFail)
        {
            UE_LOG(LogTemp, Warning, TEXT(">> [로봇 연속동작 중 주춤]: 액션 '%s' 숙련도 부족으로 건너뜁니다!"), *SubAction.mActionId.ToString());
            continue;
        }

        FBotActionCommand Command;
        Command.mActionId = SubAction.mActionId;
        Command.mDirection = SubAction.mDirection;
        Command.mSpeedMultiplier = ActualSpeed;
        Command.mDuration = ActualDuration;

        ActionSequence.Add(Command);
    }

    if (!ActionSequence.IsEmpty())
    {
        mbIsContinuousMoving = false;
        mActiveAction->ClearQueue();
        mActiveAction->EnqueueActionSequence(ActionSequence);

        UE_LOG(LogTemp, Log, TEXT("========================================================================="));
        UE_LOG(LogTemp, Log, TEXT(">> [연속 음성 파이프라인 (총 %d단 연속 액션 시퀀스)]"), ActionSequence.Num());
        UE_LOG(LogTemp, Log, TEXT(" - 1. VAD 음성 활동 감지   : %.1f ms"), VadDurationMs);
        UE_LOG(LogTemp, Log, TEXT(" - 2. STT 텍스트 디코딩    : %.1f ms (원문: '%s')"), SttDurationMs, *RawText);
        UE_LOG(LogTemp, Log, TEXT(" - 3. 온디바이스 인코더 추론 : %.1f ms"), InferDurationMs);
        UE_LOG(LogTemp, Log, TEXT(" - 4. 액션 시퀀스 큐잉 등록 : %.1f ms"), ActionDispatchMs);
        UE_LOG(LogTemp, Log, TEXT(" ★ [체감 반응 지연]: %.1f ms"), TotalLatencyFromSpeechEnd);
        UE_LOG(LogTemp, Log, TEXT("========================================================================="));
    }
}

void UVKVoiceCommanderComponent::ExecuteCommandOnGameThread(EBotVoiceCommand Command, FString const& RawText)
{
    if (!IsValid(this))
    {
        return;
    }

    mActiveCommand = Command;
    UE_LOG(LogTemp, Log, TEXT("[VoiceCommander] 명령 처리 완료: %s (원문: %s)"), *UEnum::GetValueAsString(Command), *RawText);
    OnVoiceCommandExecuted.Broadcast(Command, RawText);
}

void UVKVoiceCommanderComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (mVoicePipeline && !mVoicePipeline->IsCapturing())
    {
        mMicrophoneRetryTimer += DeltaTime;
        if (mMicrophoneRetryTimer >= 2.0f)
        {
            mMicrophoneRetryTimer = 0.0f;
            if (mVoicePipeline->StartCapture())
            {
                UE_LOG(LogTemp, Log, TEXT(">> [마이크 자동 감지 및 연결 성공]: 음성 캡처가 활성화되었습니다!"));
            }
        }
    }

    if (!mOwnerCharacter.IsValid())
    {
        return;
    }

    if (mbIsContinuousMoving && !mContinuousDirection.IsNearlyZero())
    {
        mOwnerCharacter->DoMove(mContinuousDirection.Y * mContinuousSpeedMultiplier, mContinuousDirection.X * mContinuousSpeedMultiplier);
        return;
    }

    if (mActiveAction != nullptr && mActiveAction->IsActionActive())
    {
        FVector const CurrentDir = mActiveAction->GetCurrentDirection();
        float const SpeedMult = mActiveAction->GetCurrentSpeedMultiplier();
        mOwnerCharacter->DoMove(CurrentDir.Y * SpeedMult, CurrentDir.X * SpeedMult);
    }
}
