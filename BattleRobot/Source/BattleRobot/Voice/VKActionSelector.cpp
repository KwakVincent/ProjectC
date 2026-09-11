#include "VKActionSelector.h"
#include "VKActionPoolComponent.h"
#include "VKEmbeddingEncoderRunner.h"

UVKActionSelector::UVKActionSelector() = default;

FBotActionParseResult UVKActionSelector::ClassifyVoiceCommand(
    FString const& RawVoiceText,
    UVKActionPoolComponent const* ActionPool,
    FVKEmbeddingEncoderRunner* EncoderRunner,
    float const SimilarityThreshold)
{
    FBotActionParseResult Result;
    Result.mbSuccess = false;

    FString TrimmedText = RawVoiceText;
    TrimmedText.TrimStartAndEndInline();
    if (TrimmedText.IsEmpty())
    {
        return Result;
    }

    FName ResolvedActionId = NAME_None;
    float ResolvedSimilarity = -1.0f;

    if (EncoderRunner != nullptr && EncoderRunner->IsInitialized() && ActionPool != nullptr)
    {
        TArray<float> QueryEmbedding;
        if (EncoderRunner->GetSentenceEmbedding(TrimmedText, QueryEmbedding))
        {
            ActionPool->FindBestMatchingAction(QueryEmbedding, SimilarityThreshold, ResolvedActionId, ResolvedSimilarity);
        }
    }

    if (ResolvedActionId.IsNone())
    {
        if (TrimmedText.Contains(TEXT("정지")) || TrimmedText.Contains(TEXT("멈춰")) || TrimmedText.Contains(TEXT("스톱")) || TrimmedText.Contains(TEXT("stop")))
        {
            ResolvedActionId = FName(TEXT("Stop"));
        }
        else if (TrimmedText.Contains(TEXT("대시")) || TrimmedText.Contains(TEXT("돌진")) || TrimmedText.Contains(TEXT("뛰어")) || TrimmedText.Contains(TEXT("dash")))
        {
            ResolvedActionId = FName(TEXT("Dash"));
        }
        else if (TrimmedText.Contains(TEXT("회피")) || TrimmedText.Contains(TEXT("구르")) || TrimmedText.Contains(TEXT("evade")))
        {
            ResolvedActionId = FName(TEXT("Evade"));
        }
        else if (TrimmedText.Contains(TEXT("후퇴")) || TrimmedText.Contains(TEXT("물러서")) || TrimmedText.Contains(TEXT("빠져")))
        {
            ResolvedActionId = FName(TEXT("FallBack"));
        }
        else
        {
            ResolvedActionId = FName(TEXT("Move"));
        }
    }

    Result.mActionId = ResolvedActionId;
    Result.mDirection = ExtractDirectionFromText(TrimmedText, ResolvedActionId);
    Result.mDeltaTicks = ExtractDeltaTicksFromText(TrimmedText);
    Result.mbSuccess = true;

    return Result;
}

FVector UVKActionSelector::ExtractDirectionFromText(FString const& Text, FName const& ActionId)
{
    if (ActionId == FName(TEXT("Stop")))
    {
        return FVector::ZeroVector;
    }

    if (ActionId == FName(TEXT("FallBack")))
    {
        return FVector(-1.0f, 0.0f, 0.0f);
    }

    FString const Lower = Text.ToLower();
    if (Lower.Contains(TEXT("뒤")) || Lower.Contains(TEXT("후진")) || Lower.Contains(TEXT("back")))
    {
        return FVector(-1.0f, 0.0f, 0.0f);
    }

    if (Lower.Contains(TEXT("왼")) || Lower.Contains(TEXT("좌")) || Lower.Contains(TEXT("left")))
    {
        return FVector(0.0f, -1.0f, 0.0f);
    }

    if (Lower.Contains(TEXT("오른")) || Lower.Contains(TEXT("우")) || Lower.Contains(TEXT("right")))
    {
        return FVector(0.0f, 1.0f, 0.0f);
    }

    return FVector(1.0f, 0.0f, 0.0f);
}

int32 UVKActionSelector::ExtractDeltaTicksFromText(FString const& Text)
{
    if (Text.Contains(TEXT("조금만")) || Text.Contains(TEXT("살짝만")) || Text.Contains(TEXT("살짝")) || Text.Contains(TEXT("조금")))
    {
        return 1;
    }

    if (Text.Contains(TEXT("더")) || Text.Contains(TEXT("계속")) || Text.Contains(TEXT("훨씬")) || Text.Contains(TEXT("길게")))
    {
        return 3;
    }

    return 0;
}


FString UVKActionSelector::BuildPrompt(FString const& UserVoiceText, UVKActionPoolComponent const* ActionPool)
{
    if (UserVoiceText.IsEmpty())
    {
        return FString();
    }

    FString CatalogText;
    if (ActionPool != nullptr)
    {
        CatalogText = ActionPool->BuildLLMActionCatalog();
    }

    FString Prompt = TEXT("당신은 로봇 이동 제어기입니다. 주어진 음성 명령을 분석하여 반드시 '[ActionId] [Direction] [DeltaTicks]' 형식의 3단어로만 답변하십시오.\n\n");
    Prompt += TEXT("[사용 가능한 Actions]\n");
    Prompt += CatalogText;
    Prompt += TEXT("\n[사용 가능한 Directions]\nForward, Backward, Left, Right, None\n\n");
    Prompt += TEXT("[DeltaTicks 규칙 (1 Tick = 0.1초 조절)]\n");
    Prompt += TEXT("+0: 시간 변경 없음 (보통 명령)\n");
    Prompt += TEXT("+1: 살짝 더, 조금만 더 (+0.1초)\n");
    Prompt += TEXT("+3: 좀 더 가, 더 길게 (+0.3초)\n");
    Prompt += TEXT("+5: 훨씬 더, 멀리 가 (+0.5초)\n");
    Prompt += TEXT("-1: 살짝만, 조금 덜 (-0.1초)\n");
    Prompt += TEXT("-3: 너무 많이 갔어, 짧게 (-0.3초)\n\n");
    Prompt += TEXT("[예시]\n");
    Prompt += TEXT("명령: 앞으로 가\n답변: Move Forward +0\n");
    Prompt += TEXT("명령: 앞으로 조금만 더 가\n답변: Move Forward +1\n");
    Prompt += TEXT("명령: 우측으로 훨씬 길게 대시해\n답변: Dash Right +5\n");
    Prompt += TEXT("명령: 뒤로 살짝만 물러서\n답변: Move Backward -1\n");
    Prompt += TEXT("명령: 당장 멈춰\n답변: Stop None +0\n\n");
    Prompt += FString::Printf(TEXT("명령: %s\n답변:"), *UserVoiceText);

    return Prompt;
}

FVector UVKActionSelector::ConvertDirectionToVector(FString const& DirectionString)
{
    FString const Lower = DirectionString.ToLower();
    if (Lower.Contains(TEXT("forward")) || Lower.Contains(TEXT("front")))
    {
        return FVector(1.0f, 0.0f, 0.0f);
    }

    if (Lower.Contains(TEXT("backward")) || Lower.Contains(TEXT("back")))
    {
        return FVector(-1.0f, 0.0f, 0.0f);
    }

    if (Lower.Contains(TEXT("left")))
    {
        return FVector(0.0f, -1.0f, 0.0f);
    }

    if (Lower.Contains(TEXT("right")))
    {
        return FVector(0.0f, 1.0f, 0.0f);
    }

    return FVector::ZeroVector;
}

bool UVKActionSelector::ParseTokenResponse(
    FString const& ResponseText,
    UVKActionPoolComponent const* ActionPool,
    FName& OutActionId,
    FVector& OutDirection,
    int32& OutDeltaTicks)
{
    FBotActionParseResult const ParseResult = ParseTokens(ResponseText, ActionPool);
    if (!ParseResult.mbSuccess)
    {
        return false;
    }

    OutActionId = ParseResult.mActionId;
    OutDirection = ParseResult.mDirection;
    OutDeltaTicks = ParseResult.mDeltaTicks;
    return true;
}

FBotActionParseResult UVKActionSelector::ParseTokens(FString const& ResponseText, UVKActionPoolComponent const* ActionPool)
{
    FBotActionParseResult Result;
    Result.mbSuccess = false;

    if (ResponseText.IsEmpty())
    {
        return Result;
    }

    FString Trimmed = ResponseText;
    Trimmed.TrimStartAndEndInline();

    TArray<FString> Tokens;
    Trimmed.ParseIntoArrayWS(Tokens);
    if (Tokens.Num() == 0)
    {
        return Result;
    }

    FName const CandidateActionId = FName(*Tokens[0]);
    if (ActionPool != nullptr)
    {
        FBotMovementActionDef ActionDef;
        if (!ActionPool->GetAction(CandidateActionId, ActionDef) || !ActionDef.mbIsUnlocked)
        {
            return Result;
        }
    }

    Result.mActionId = CandidateActionId;

    if (Tokens.Num() > 1)
    {
        Result.mDirection = ConvertDirectionToVector(Tokens[1]);
    }
    else
    {
        Result.mDirection = FVector(1.0f, 0.0f, 0.0f);
    }

    Result.mDeltaTicks = 0;
    if (Tokens.Num() > 2)
    {
        FString TickToken = Tokens[2];
        TickToken.RemoveFromStart(TEXT("+"));
        Result.mDeltaTicks = FCString::Atoi(*TickToken);
    }

    Result.mbSuccess = true;
    return Result;
}
