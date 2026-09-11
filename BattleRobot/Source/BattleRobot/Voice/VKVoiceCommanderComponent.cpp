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

    FString const VadPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Models/silero_vad.onnx"));
    FString const WhisperPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Models/ggml-base.bin"));

    mVoicePipeline = MakeUnique<FVoicePipeline>(VadPath, WhisperPath);
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

        FBotActionParseResult const ParseResult = UVKActionSelector::ClassifyVoiceCommand(
            RecognizedText, mActionPool, mEncoderRunner.Get(), 0.35f);

        double const InferDurationMs = (FPlatformTime::Seconds() - InferStart) * 1000.0;
        double const DispatchStartTime = FPlatformTime::Seconds();

        FName const ActionId = ParseResult.mActionId;
        FVector const Direction = ParseResult.mDirection;
        int32 const DeltaTicks = ParseResult.mDeltaTicks;

        AsyncTask(ENamedThreads::GameThread, [this, ActionId, Direction, DeltaTicks, RecognizedText, VadDurationMs, SttDurationMs, InferDurationMs, DispatchStartTime]()
        {
            ExecuteActionOnGameThread(ActionId, Direction, DeltaTicks, RecognizedText, VadDurationMs, SttDurationMs, InferDurationMs, DispatchStartTime);
        });
    });
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
