#pragma once

#include "Database/CoDDatabaseService.h"
#include "Interface/IGameAssetDiscoverer.h"
#include "Interface/IMemoryReader.h"
#include "WraithX/CoDAssetType.h"
#include "WraithX/GameProcess.h"

class UWraithSettings;
class UWraithSettingsManager;

class FModernWarfare6AssetDiscoverer : public IGameAssetDiscoverer
{
public:
	FModernWarfare6AssetDiscoverer();
	virtual ~FModernWarfare6AssetDiscoverer() override = default;

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
	// --- Asset Pool Processing Logic ---

	// Reads the FXAssetPool64 structure
	bool ReadAssetPoolHeader(int32 PoolIdentifier, FXAssetPool64& OutPoolHeader);
	// Reads the FXAsset64 structure
	bool ReadAssetNode(uint64 AssetNodePtr, FXAsset64& OutAssetNode);
	// Processes a generic asset node using the database and callbacks
	template <typename TGameAssetStruct, typename TCoDAssetType>
	void ProcessGenericAssetNode(FXAsset64 AssetNode,
	                             EWraithAssetType WraithType,
	                             const TCHAR* DefaultPrefix,
	                             TFunction<void(const TGameAssetStruct& AssetHeader,
	                                            TSharedPtr<TCoDAssetType> CoDAsset)> Customizer,
	                             FAssetDiscoveredDelegate OnAssetDiscovered);
	// Specific asset processing functions called by DiscoverAssetsInPool
	void DiscoverModelAssets(FXAsset64 AssetNode, FAssetDiscoveredDelegate OnAssetDiscovered);
	void DiscoverImageAssets(FXAsset64 AssetNode, FAssetDiscoveredDelegate OnAssetDiscovered);
	void DiscoverAnimAssets(FXAsset64 AssetNode, FAssetDiscoveredDelegate OnAssetDiscovered);
	void DiscoverMaterialAssets(FXAsset64 AssetNode, FAssetDiscoveredDelegate OnAssetDiscovered);
	void DiscoverSoundAssets(FXAsset64 AssetNode, FAssetDiscoveredDelegate OnAssetDiscovered);
	void DiscoverMapAssets(FXAsset64 AssetNode, FAssetDiscoveredDelegate OnAssetDiscovered);

	// Helper to clean up asset names
	static FString ProcessAssetName(const FString& Name);

	IMemoryReader* Reader = nullptr;
	TSharedPtr<LocateGameInfo::FParasyteBaseState> ParasyteState;
	CoDAssets::ESupportedGames GameType = CoDAssets::ESupportedGames::None;
	CoDAssets::ESupportedGameFlags GameFlag = CoDAssets::ESupportedGameFlags::None;
	TSharedPtr<FXSub> XSubDecrypt;
	TUniquePtr<FCoDCDNDownloader> CDNDownloader;
};

template <typename TGameAssetStruct, typename TCoDAssetType>
void FModernWarfare6AssetDiscoverer::ProcessGenericAssetNode(
	FXAsset64 AssetNode, EWraithAssetType WraithType,
	const TCHAR* DefaultPrefix,
	TFunction<void(const TGameAssetStruct& AssetHeader, TSharedPtr<TCoDAssetType> CoDAsset)> Customizer,
	FAssetDiscoveredDelegate OnAssetDiscovered)
{
	TGameAssetStruct AssetHeader;
	if (!Reader->ReadMemory<TGameAssetStruct>(AssetNode.Header, AssetHeader))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to read asset header struct at 0x%llX"), AssetNode.Header);
		return;
	}

	uint64 AssetHash = AssetHeader.Hash & 0xFFFFFFFFFFFFFFF;

	FCoDDatabaseService::Get().GetAssetNameAsync(
		AssetHash, [this, AssetHeader, NodeHeader = AssetNode.Header, NodeTemp = AssetNode.Temp, WraithType,
			DefaultPrefix = FString(DefaultPrefix), Customizer = MoveTemp(Customizer),
			OnAssetDiscovered, AssetHash](TOptional<FString> QueryName)
		{
			TSharedPtr<TCoDAssetType> LoadedAsset = MakeShared<TCoDAssetType>();
			LoadedAsset->AssetType = WraithType;
			LoadedAsset->AssetName = QueryName.IsSet()
				                         ? QueryName.GetValue()
				                         : FString::Printf(TEXT("%s_%llx"), *DefaultPrefix, AssetHash);
			LoadedAsset->AssetPointer = NodeHeader;
			LoadedAsset->AssetStatus = EWraithAssetStatus::Loaded;

			if constexpr (std::is_same_v<TCoDAssetType, FCoDAnim>)
			{
				LoadedAsset->AssetStatus = NodeTemp == 1
					                           ? EWraithAssetStatus::Placeholder
					                           : EWraithAssetStatus::Loaded;
			}
			Customizer(AssetHeader, LoadedAsset);

			OnAssetDiscovered.ExecuteIfBound(StaticCastSharedPtr<FCoDAsset>(LoadedAsset));
		});
}
