#pragma once
#include "Interface/IGameAssetDiscoverer.h"

struct FXAsset64;
struct FXAssetPool64;

class FModernWarfare5AssetDiscoverer : public IGameAssetDiscoverer
{
public:
	FModernWarfare5AssetDiscoverer();
	virtual ~FModernWarfare5AssetDiscoverer() override = default;
	virtual bool Initialize(IMemoryReader* InReader,
	                        const CoDAssets::FCoDGameProcess& InProcessInfo,
	                        TSharedPtr<LocateGameInfo::FParasyteBaseState> InParasyteState) override;

	virtual CoDAssets::ESupportedGames GetGameType() const override { return GameType; }
	virtual CoDAssets::ESupportedGameFlags GetGameFlags() const override { return GameFlag; }

	virtual TSharedPtr<FXSub> GetDecryptor() const override { return XSubDecrypt; }
	virtual FCoDCDNDownloader* GetCDNDownloader() const override { return CDNDownloader.Get(); }

	virtual TArray<FAssetPoolDefinition> GetAssetPools() const override;

	virtual int32 DiscoverAssetsInPool(const FAssetPoolDefinition& PoolDefinition,
	                                   FAssetDiscoveredDelegate OnAssetDiscovered) override;

	virtual bool LoadStringTableEntry(uint64 Index, FString& OutString) override;

private:
	bool ReadAssetPoolHeader(int32 PoolIdentifier, FXAssetPool64& OutPoolHeader);
	bool ReadAssetNode(uint64 AssetNodePtr, FXAsset64& OutAssetNode);

	void DiscoverModelAssets(FXAsset64 AssetNode, FAssetDiscoveredDelegate OnAssetDiscovered);
	void DiscoverImageAssets(FXAsset64 AssetNode, FAssetDiscoveredDelegate OnAssetDiscovered);
	void DiscoverAnimAssets(FXAsset64 AssetNode, FAssetDiscoveredDelegate OnAssetDiscovered);
	void DiscoverMaterialAssets(FXAsset64 AssetNode, FAssetDiscoveredDelegate OnAssetDiscovered);
	void DiscoverSoundAssets(FXAsset64 AssetNode, FAssetDiscoveredDelegate OnAssetDiscovered);
	void DiscoverMapAssets(FXAsset64 AssetNode, FAssetDiscoveredDelegate OnAssetDiscovered);
	
	TSharedPtr<LocateGameInfo::FParasyteBaseState> ParasyteState;
	
	CoDAssets::ESupportedGames GameType = CoDAssets::ESupportedGames::None;
	CoDAssets::ESupportedGameFlags GameFlag = CoDAssets::ESupportedGameFlags::None;

	TSharedPtr<FXSub> XSubDecrypt;
	TUniquePtr<FCoDCDNDownloader> CDNDownloader;
};
