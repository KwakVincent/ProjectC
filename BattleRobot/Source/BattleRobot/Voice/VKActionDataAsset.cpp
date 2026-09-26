#include "VKActionDataAsset.h"

UVKActionDataAsset::UVKActionDataAsset() = default;

TArray<FBotMovementActionDef> const& UVKActionDataAsset::GetActions() const
{
	return mActions;
}

TArray<FVoiceCommandMapping> const& UVKActionDataAsset::GetCommandMappings() const
{
	return mCommandMappings;
}
