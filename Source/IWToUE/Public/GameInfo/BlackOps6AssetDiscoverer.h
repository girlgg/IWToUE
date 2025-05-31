#pragma once
#include "Interface/IGameAssetDiscoverer.h"

class FBlackOps6AssetDiscoverer : public IGameAssetDiscoverer
{
public:
	FBlackOps6AssetDiscoverer();
	virtual ~FBlackOps6AssetDiscoverer() override;

	virtual bool Initialize(IMemoryReader* InReader, const CoDAssets::FCoDGameProcess& InProcessInfo,
	                        TSharedPtr<LocateGameInfo::FParasyteBaseState> InParasyteState) override;

	virtual CoDAssets::ESupportedGames GetGameType() const override;
	virtual CoDAssets::ESupportedGameFlags GetGameFlags() const override;

	virtual TSharedPtr<FXSub> GetDecryptor() const override;
	virtual FCoDCDNDownloader* GetCDNDownloader() const override;

	virtual TArray<FAssetPoolDefinition> GetAssetPools() const override;

	virtual int32 DiscoverAssetsInPool(const FAssetPoolDefinition& PoolDefinition,
	                                   FAssetDiscoveredDelegate OnAssetDiscovered) override;

	virtual bool LoadStringTableEntry(uint64 Index, FString& OutString) override;
};
