#include "VKActionSpeechLearner.h"

bool FVKActionSpeechLearner::ParseActionFromSpeech(
    FString const& SpeechText,
    FBotMovementActionDef& OutActionDef,
    TArray<FString>& OutTriggerPhrases)
{
    FString Trimmed = SpeechText;
    Trimmed.TrimStartAndEndInline();
    if (Trimmed.IsEmpty())
    {
        return false;
    }

    FString ActionName;
    int32 DelimIndex = INDEX_NONE;

    TArray<TCHAR> const Delims = { TEXT('!'), TEXT('?'), TEXT('.'), TEXT(','), TEXT(':'), TEXT('~') };
    for (TCHAR const Delim : Delims)
    {
        int32 const FoundIdx = Trimmed.Find(FString(1, &Delim));
        if (FoundIdx != INDEX_NONE && (DelimIndex == INDEX_NONE || FoundIdx < DelimIndex))
        {
            DelimIndex = FoundIdx;
        }
    }

    if (DelimIndex != INDEX_NONE && DelimIndex > 0)
    {
        ActionName = Trimmed.Left(DelimIndex).TrimStartAndEnd();
    }
    else
    {
        TArray<FString> Words;
        Trimmed.ParseIntoArrayWS(Words);
        if (Words.Num() > 0)
        {
            ActionName = Words[0];
        }
    }

    if (ActionName.IsEmpty())
    {
        return false;
    }

    float SpeedMultiplier = 1.5f;
    if (Trimmed.Contains(TEXT("부스팅")) || Trimmed.Contains(TEXT("전속")) || Trimmed.Contains(TEXT("최대")) || Trimmed.Contains(TEXT("빠르게")) || Trimmed.Contains(TEXT("돌진")))
    {
        SpeedMultiplier = 3.0f;
    }
    else if (Trimmed.Contains(TEXT("천천히")) || Trimmed.Contains(TEXT("조심")) || Trimmed.Contains(TEXT("슬로우")))
    {
        SpeedMultiplier = 0.6f;
    }

    bool bKeepUntilStop = false;
    float Duration = 2.0f;

    if (Trimmed.Contains(TEXT("스톱 전까지")) || Trimmed.Contains(TEXT("멈출 때까지")) || Trimmed.Contains(TEXT("스톱전까지")) || Trimmed.Contains(TEXT("멈출때까지")) || Trimmed.Contains(TEXT("계속")))
    {
        bKeepUntilStop = true;
        Duration = 0.0f;
    }
    else
    {
        for (int32 Sec = 1; Sec <= 10; ++Sec)
        {
            FString const SecStr = FString::Printf(TEXT("%d초"), Sec);
            if (Trimmed.Contains(SecStr))
            {
                Duration = static_cast<float>(Sec);
                break;
            }
        }
    }

    OutActionDef.mActionId = FName(*ActionName);
    OutActionDef.mDisplayName = ActionName;
    OutActionDef.mDescription = Trimmed;
    OutActionDef.mSpeedMultiplier = SpeedMultiplier;
    OutActionDef.mDefaultDuration = Duration;
    OutActionDef.mAdaptiveDuration = Duration;
    OutActionDef.mTargetSpeedMultiplier = SpeedMultiplier;
    OutActionDef.mTargetDuration = Duration;
    OutActionDef.mbKeepUntilStop = bKeepUntilStop;
    OutActionDef.mProficiency = 0.5f;
    OutActionDef.mSuccessfulUses = 0;
    OutActionDef.mbIsUnlocked = true;
    OutActionDef.mbCanTurn = true;

    OutTriggerPhrases.Empty();
    OutTriggerPhrases.Add(ActionName);
    OutTriggerPhrases.Add(Trimmed);

    return true;
}
