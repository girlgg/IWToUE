#include "GameInfo/BlackOps6AssetDiscoverer.h"

FBlackOps6AssetDiscoverer::FBlackOps6AssetDiscoverer()
{
}

FBlackOps6AssetDiscoverer::~FBlackOps6AssetDiscoverer()
{
}

bool FBlackOps6AssetDiscoverer::Initialize(IMemoryReader* InReader, const CoDAssets::FCoDGameProcess& InProcessInfo,
	TSharedPtr<LocateGameInfo::FParasyteBaseState> InParasyteState)
{
	return false;
}

CoDAssets::ESupportedGames FBlackOps6AssetDiscoverer::GetGameType() const
{
	return CoDAssets::ESupportedGames::None;
}

CoDAssets::ESupportedGameFlags FBlackOps6AssetDiscoverer::GetGameFlags() const
{
	return CoDAssets::ESupportedGameFlags::None;
}

TSharedPtr<FXSub> FBlackOps6AssetDiscoverer::GetDecryptor() const
{
	return TSharedPtr<FXSub>();
}

FCoDCDNDownloader* FBlackOps6AssetDiscoverer::GetCDNDownloader() const
{
	return nullptr;
}

TArray<FAssetPoolDefinition> FBlackOps6AssetDiscoverer::GetAssetPools() const
{
	return TArray<FAssetPoolDefinition>();
}

int32 FBlackOps6AssetDiscoverer::DiscoverAssetsInPool(const FAssetPoolDefinition& PoolDefinition,
	FAssetDiscoveredDelegate OnAssetDiscovered)
{
	return 0;
}

bool FBlackOps6AssetDiscoverer::LoadStringTableEntry(uint64 Index, FString& OutString)
{
	return false;
}
