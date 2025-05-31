#include "GameInfo/GameAssetHandlerFactory.h"

#include "GameInfo/ModernWarfare6AssetHandler.h"
#include "GameInfo/ModernWarfare5AssetHandler.h"
#include "Structures/CodAssets.h"
#include "WraithX/GameProcess.h"

TSharedPtr<IGameAssetHandler> FGameAssetHandlerFactory::CreateHandler(TSharedPtr<FGameProcess> ProcessInstance)
{
	if (!ProcessInstance.IsValid())
	{
		return nullptr;
	}

	switch (CoDAssets::ESupportedGames GameType = ProcessInstance->GetCurrentGameType())
	{
	case CoDAssets::ESupportedGames::ModernWarfare5:
		return MakeShared<ModernWarfare5AssetHandler>(ProcessInstance);
	case CoDAssets::ESupportedGames::ModernWarfare6:
		return MakeShared<FModernWarfare6AssetHandler>(ProcessInstance);
	default:
		UE_LOG(LogTemp, Warning, TEXT("Unsupported game type for asset handling: %d"), static_cast<int32>(GameType));
		return nullptr;
	}
}
