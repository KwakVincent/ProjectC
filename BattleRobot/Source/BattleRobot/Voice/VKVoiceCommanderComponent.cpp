#include "VKVoiceCommanderComponent.h"
#include "VoicePipeline.h"
#include "BattleRobotCharacter.h"
#include "VKActionPoolComponent.h"
#include "VKActiveActionComponent.h"
#include "VKActionSelector.h"
#include "QwenOnDeviceRunner.h"
#include "Misc/Paths.h"
#include "Async/Async.h"

UVKVoiceCommanderComponent::UVKVoiceCommanderComponent()
    : mMoveDuration(2.0f)
    , mActionPool(nullptr)
    , mActiveAction(nullptr)
    , mQwenRunner(nullptr)
    , mActiveCommand(EBotVoiceCommand::None)
    , mRemainingMoveTime(0.0f)
{
    PrimaryComponentTick.bCanEverTick = true;
}

UVKVoiceCommanderComponent::~UVKVoiceCommanderComponent() = default;

UVKActionPoolComponent* UVKVoiceCommanderComponent::GetActionPool() const
{
    return mActionPool;
}

UVKActiveActionComponent* UVKVoiceCommanderComponent::GetActiveAction() const
{
    return mActiveAction;
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

    mQwenRunner = MakeUnique<FQwenOnDeviceRunner>();
    FString const QwenModelPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Models/Qwen2.5_0.5b_onnx"));
    Async(EAsyncExecution::Thread, [this, QwenModelPath]()
    {
        if (mQwenRunner)
        {
            mQwenRunner->Initialize(QwenModelPath);
        }
    });

    StartVoiceRecognition();
}

void UVKVoiceCommanderComponent::EndPlay(EEndPlayReason::Type const EndPlayReason)
{
    StopVoiceRecognition();

    if (mQwenRunner)
    {
        mQwenRunner->Shutdown();
        mQwenRunner.Reset();
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

    Async(EAsyncExecution::Thread, [this, RecognizedText, VadDurationMs, SttDurationMs]()
    {
        FName ActionId = FName(TEXT("Move"));
        FVector Direction = FVector(1.0f, 0.0f, 0.0f);
        int32 DeltaTicks = 0;
        bool bHandledByLLM = false;

        double const InferStart = FPlatformTime::Seconds();

        if (mQwenRunner && mQwenRunner->IsInitialized())
        {
            FString const Prompt = UVKActionSelector::BuildPrompt(RecognizedText, mActionPool);
            FString const Response = mQwenRunner->GenerateText(Prompt, 10);
            UE_LOG(LogTemp, Log, TEXT(">> [Qwen2.5 On-Device 응답]: %s"), *Response);

            if (UVKActionSelector::ParseTokenResponse(Response, mActionPool, ActionId, Direction, DeltaTicks))
            {
                bHandledByLLM = true;
            }
        }

        if (!bHandledByLLM)
        {
            if (RecognizedText.Contains(TEXT("정지")) || RecognizedText.Contains(TEXT("멈춰")) || RecognizedText.Contains(TEXT("스톱")) || RecognizedText.Contains(TEXT("stop")))
            {
                ActionId = FName(TEXT("Stop"));
                Direction = FVector::ZeroVector;
            }
            else if (RecognizedText.Contains(TEXT("대시")) || RecognizedText.Contains(TEXT("돌진")) || RecognizedText.Contains(TEXT("뛰어")) || RecognizedText.Contains(TEXT("dash")))
            {
                ActionId = FName(TEXT("Dash"));
            }
            else if (RecognizedText.Contains(TEXT("회피")) || RecognizedText.Contains(TEXT("구르")) || RecognizedText.Contains(TEXT("evade")))
            {
                ActionId = FName(TEXT("Evade"));
            }
            else if (RecognizedText.Contains(TEXT("후퇴")) || RecognizedText.Contains(TEXT("물러서")) || RecognizedText.Contains(TEXT("빠져")))
            {
                ActionId = FName(TEXT("FallBack"));
            }

            if (ActionId != FName(TEXT("Stop")))
            {
                if (RecognizedText.Contains(TEXT("뒤")) || RecognizedText.Contains(TEXT("후진")))
                {
                    Direction = FVector(-1.0f, 0.0f, 0.0f);
                }
                else if (RecognizedText.Contains(TEXT("왼")) || RecognizedText.Contains(TEXT("좌")))
                {
                    Direction = FVector(0.0f, -1.0f, 0.0f);
                }
                else if (RecognizedText.Contains(TEXT("오른")) || RecognizedText.Contains(TEXT("우")))
                {
                    Direction = FVector(0.0f, 1.0f, 0.0f);
                }
            }

            if (RecognizedText.Contains(TEXT("조금만")) || RecognizedText.Contains(TEXT("살짝만")))
            {
                DeltaTicks = 1;
            }
            else if (RecognizedText.Contains(TEXT("더")) || RecognizedText.Contains(TEXT("계속")))
            {
                DeltaTicks = 3;
            }
        }

        double const InferDurationMs = (FPlatformTime::Seconds() - InferStart) * 1000.0;
        double const DispatchStartTime = FPlatformTime::Seconds();

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

    float ExecDuration = mMoveDuration;
    if (mActionPool != nullptr)
    {
        if (DeltaTicks != 0)
        {
            float const UpdatedDuration = mActionPool->AdjustActionDuration(ActionId, DeltaTicks);
            UE_LOG(LogTemp, Log, TEXT(">> [적응형 지속시간 학습]: 액션 %s, Tick 변화: %d (%.1f초), 갱신된 지속시간: %.2f초"),
                *ActionId.ToString(), DeltaTicks, DeltaTicks * 0.1f, UpdatedDuration);
        }

        FBotMovementActionDef ActionDef;
        if (mActionPool->GetAction(ActionId, ActionDef))
        {
            ExecDuration = ActionDef.mAdaptiveDuration;
            mActiveAction->ApplyAction(ActionId, Direction, ActionDef.mSpeedMultiplier, ExecDuration);
        }
        else
        {
            mActiveAction->ApplyAction(ActionId, Direction, 1.0f, ExecDuration);
        }
    }
    else
    {
        mActiveAction->ApplyAction(ActionId, Direction, 1.0f, ExecDuration);
    }

    UE_LOG(LogTemp, Log, TEXT("========================================================================="));
    UE_LOG(LogTemp, Log, TEXT(">> [음성 파이프라인 지연 시간 프로파일링 리포트]"));
    UE_LOG(LogTemp, Log, TEXT(" - 1. VAD 음성 활동 감지   : %.1f ms"), VadDurationMs);
    UE_LOG(LogTemp, Log, TEXT(" - 2. STT 텍스트 디코딩    : %.1f ms (원문: '%s')"), SttDurationMs, *RawText);
    UE_LOG(LogTemp, Log, TEXT(" - 3. Qwen2.5 온디바이스 추론 : %.1f ms (결과: %s, 시간: %.2f초, Ticks: %+d)"),
        InferDurationMs, *ActionId.ToString(), ExecDuration, DeltaTicks);
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

    if (!mOwnerCharacter.IsValid() || mActiveAction == nullptr || !mActiveAction->IsActionActive())
    {
        return;
    }

    FVector const CurrentDir = mActiveAction->GetCurrentDirection();
    float const SpeedMult = mActiveAction->GetCurrentSpeedMultiplier();

    mOwnerCharacter->DoMove(CurrentDir.Y * SpeedMult, CurrentDir.X * SpeedMult);
}
