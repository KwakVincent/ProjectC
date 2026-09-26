#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VKActionPoolComponent.h"
#include "VKActionDataAsset.generated.h"

UCLASS(BlueprintType)
class BATTLEROBOT_API UVKActionDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UVKActionDataAsset();

	TArray<FBotMovementActionDef> const& GetActions() const;
	TArray<FVoiceCommandMapping> const& GetCommandMappings() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="VoiceActions")
	TArray<FBotMovementActionDef> mActions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="VoiceActions")
	TArray<FVoiceCommandMapping> mCommandMappings;
};
