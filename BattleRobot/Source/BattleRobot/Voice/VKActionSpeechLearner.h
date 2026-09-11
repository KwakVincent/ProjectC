#pragma once

#include "CoreMinimal.h"
#include "VKActionPoolComponent.h"

class BATTLEROBOT_API FVKActionSpeechLearner
{
public:
    static bool ParseActionFromSpeech(
        FString const& SpeechText,
        FBotMovementActionDef& OutActionDef,
        TArray<FString>& OutTriggerPhrases);
};
