#pragma once
#include "ModernWarfare6AssetDiscoverer.h"
#include "WraithX/LocateGameInfo.h"

class FGameAssetDiscovererFactory
{
public:
	static TSharedPtr<IGameAssetDiscoverer> CreateDiscoverer(
		const CoDAssets::FCoDGameProcess& ProcessInfo,
		TSharedPtr<LocateGameInfo::FParasyteBaseState> ParasyteState)
	{
		uint64 GameIdToUse = 0;
		if (ParasyteState.IsValid() && ParasyteState->GameID != 0)
		{
			GameIdToUse = ParasyteState->GameID;
			UE_LOG(LogTemp, Log, TEXT("Factory using Parasyte GameID: 0x%llX"), GameIdToUse);
		}
		else if (ProcessInfo.GameID != CoDAssets::ESupportedGames::None)
		{
			UE_LOG(LogTemp, Warning, TEXT("Factory falling back to ProcessInfo Game enum - conversion needed."));
			GameIdToUse = 0;
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Factory cannot determine GameID."));
			return nullptr;
		}

		switch (GameIdToUse)
		{
		case 0x4B4F4D41594D4159:
			return MakeShared<FModernWarfare6AssetDiscoverer>();
		default:
			UE_LOG(LogTemp, Warning, TEXT("Unsupported GameID for Asset Discoverer: 0x%llX"), GameIdToUse);
			return nullptr;
		}
	}
};
