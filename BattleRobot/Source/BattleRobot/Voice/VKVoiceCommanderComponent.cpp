#include "VKVoiceCommanderComponent.h"
#include "VoicePipeline.h"
#include "BattleRobotCharacter.h"
#include "Misc/Paths.h"
#include "Async/Async.h"

UVKVoiceCommanderComponent::UVKVoiceCommanderComponent()
    : mMoveDuration(2.0f)
    , mActiveCommand(EBotVoiceCommand::None)
    , mRemainingMoveTime(0.0f)
{
    PrimaryComponentTick.bCanEverTick = true;
}

UVKVoiceCommanderComponent::~UVKVoiceCommanderComponent() = default;

void UVKVoiceCommanderComponent::BeginPlay()
{
    Super::BeginPlay();

    mOwnerCharacter = Cast<ABattleRobotCharacter>(GetOwner());
    mClassifier = MakeUnique<FRuleBasedCommandClassifier>();

    StartVoiceRecognition();
}

void UVKVoiceCommanderComponent::EndPlay(EEndPlayReason::Type const EndPlayReason)
{
    StopVoiceRecognition();
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
    mVoicePipeline->SetOnSpeechRecognized([this](FString const& Text)
    {
        HandleSpeechRecognized(Text);
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

void UVKVoiceCommanderComponent::HandleSpeechRecognized(FString const& RecognizedText)
{
    if (RecognizedText.IsEmpty() || !mClassifier)
    {
        return;
    }

    EBotVoiceCommand const Command = mClassifier->ClassifyCommand(RecognizedText);

    AsyncTask(ENamedThreads::GameThread, [this, Command, RecognizedText]()
    {
        ExecuteCommandOnGameThread(Command, RecognizedText);
    });
}

void UVKVoiceCommanderComponent::ExecuteCommandOnGameThread(EBotVoiceCommand Command, FString const& RawText)
{
    if (!IsValid(this))
    {
        return;
    }

    mActiveCommand = Command;
    mRemainingMoveTime = (Command == EBotVoiceCommand::Stop || Command == EBotVoiceCommand::None) ? 0.0f : mMoveDuration;

    UE_LOG(LogTemp, Log, TEXT("[VoiceCommander] 명령 수신: %s (원문: %s)"), *UEnum::GetValueAsString(Command), *RawText);
    OnVoiceCommandExecuted.Broadcast(Command, RawText);
}

void UVKVoiceCommanderComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (mRemainingMoveTime <= 0.0f || !mOwnerCharacter.IsValid())
    {
        return;
    }

    switch (mActiveCommand)
    {
        case EBotVoiceCommand::Forward:
        {
            mOwnerCharacter->DoMove(0.0f, 1.0f);
            break;
        }
        case EBotVoiceCommand::Backward:
        {
            mOwnerCharacter->DoMove(0.0f, -1.0f);
            break;
        }
        case EBotVoiceCommand::Left:
        {
            mOwnerCharacter->DoMove(-1.0f, 0.0f);
            break;
        }
        case EBotVoiceCommand::Right:
        {
            mOwnerCharacter->DoMove(1.0f, 0.0f);
            break;
        }
        default:
        {
            mRemainingMoveTime = 0.0f;
            return;
        }
    }

    mRemainingMoveTime -= DeltaTime;
}
