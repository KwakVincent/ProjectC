#include "VoiceCommandClassifier.h"

FRuleBasedCommandClassifier::FRuleBasedCommandClassifier()
{
    mForwardKeywords = { TEXT("전진"), TEXT("앞으로"), TEXT("앞"), TEXT("직진"), TEXT("가자"), TEXT("전진해"), TEXT("forward"), TEXT("go") };
    mBackwardKeywords = { TEXT("후진"), TEXT("뒤로"), TEXT("뒤"), TEXT("빠져"), TEXT("후진해"), TEXT("back"), TEXT("backward") };
    mLeftKeywords = { TEXT("좌측"), TEXT("왼쪽"), TEXT("좌"), TEXT("좌회전"), TEXT("left") };
    mRightKeywords = { TEXT("우측"), TEXT("오른쪽"), TEXT("우"), TEXT("우회전"), TEXT("right") };
    mStopKeywords = { TEXT("정지"), TEXT("멈춰"), TEXT("스톱"), TEXT("그만"), TEXT("서"), TEXT("stop") };
}

bool FRuleBasedCommandClassifier::ContainsAny(FString const& Text, TArray<FString> const& Keywords) const
{
    if (Text.IsEmpty())
    {
        return false;
    }

    for (FString const& Keyword : Keywords)
    {
        if (Text.Contains(Keyword, ESearchCase::IgnoreCase))
        {
            return true;
        }
    }

    return false;
}

EBotVoiceCommand FRuleBasedCommandClassifier::ClassifyCommand(FString const& RecognizedText)
{
    if (RecognizedText.IsEmpty())
    {
        return EBotVoiceCommand::None;
    }

    if (ContainsAny(RecognizedText, mStopKeywords))
    {
        return EBotVoiceCommand::Stop;
    }

    if (ContainsAny(RecognizedText, mForwardKeywords))
    {
        return EBotVoiceCommand::Forward;
    }

    if (ContainsAny(RecognizedText, mBackwardKeywords))
    {
        return EBotVoiceCommand::Backward;
    }

    if (ContainsAny(RecognizedText, mLeftKeywords))
    {
        return EBotVoiceCommand::Left;
    }

    if (ContainsAny(RecognizedText, mRightKeywords))
    {
        return EBotVoiceCommand::Right;
    }

    return EBotVoiceCommand::None;
}
