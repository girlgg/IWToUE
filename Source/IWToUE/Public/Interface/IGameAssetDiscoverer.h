#pragma once
#include "IMemoryReader.h"
#include "Structures/CodAssets.h"
#include "WraithX/CoDAssetType.h"

class FCoDCDNDownloader;
class FXSub;

namespace LocateGameInfo
{
	struct FParasyteBaseState;
}

namespace CoDAssets
{
	struct FCoDGameProcess;
}

class IMemoryReader;
struct FCoDAsset;
enum class EWraithAssetType : uint8;

struct FAssetPoolDefinition
{
	int32 PoolIdentifier;
	EWraithAssetType AssetType;
	FString PoolName;
};

DECLARE_DELEGATE_OneParam(FAssetDiscoveredDelegate, TSharedPtr<FCoDAsset>);
DECLARE_DELEGATE_OneParam(FDiscoveryProgressDelegate, float);

class IGameAssetDiscoverer
{
public:
	virtual ~IGameAssetDiscoverer() = default;

	virtual bool Initialize(IMemoryReader* InReader,
	                        const CoDAssets::FCoDGameProcess& InProcessInfo,
	                        TSharedPtr<LocateGameInfo::FParasyteBaseState> InParasyteState) = 0;

	virtual CoDAssets::ESupportedGames GetGameType() const = 0;
	virtual CoDAssets::ESupportedGameFlags GetGameFlags() const = 0;

	virtual TSharedPtr<FXSub> GetDecryptor() const = 0;
	virtual FCoDCDNDownloader* GetCDNDownloader() const = 0;

	virtual TArray<FAssetPoolDefinition> GetAssetPools() const = 0;

	virtual int32 DiscoverAssetsInPool(const FAssetPoolDefinition& PoolDefinition,
	                                   FAssetDiscoveredDelegate OnAssetDiscovered) = 0;

	virtual bool LoadStringTableEntry(uint64 Index, FString& OutString) = 0;

protected:
	IMemoryReader* Reader = nullptr;
};
