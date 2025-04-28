#pragma once
#include "Interface/IGameAssetDiscoverer.h"
#include "WraithX/GameProcess.h"


class IWTOUE_API FAssetDiscoveryTask : public FNonAbandonableTask
{
public:
	FAssetDiscoveryTask(TWeakPtr<FGameProcess> InOwner) : Owner(InOwner)
	{
	}

	void DoWork()
	{
		TSharedPtr<FGameProcess> OwnerPtr = Owner.Pin();
		if (!OwnerPtr || !OwnerPtr->AssetDiscoverer)
		{
			UE_LOG(LogTemp, Error, TEXT("AssetDiscoveryTask: Invalid owner or discoverer."));
			return;
		}
		UE_LOG(LogTemp, Log, TEXT("AssetDiscoveryTask: Starting discovery."));

		TArray<FAssetPoolDefinition> Pools = OwnerPtr->AssetDiscoverer->GetAssetPools();

		FAssetDiscoveredDelegate DiscoveredDelegate = FAssetDiscoveredDelegate::CreateSP(
			OwnerPtr.Get(), &FGameProcess::HandleAssetDiscovered);
		// FDiscoveryProgressDelegate ProgressDelegate = FDiscoveryProgressDelegate::CreateSP(
		// OwnerPtr.Get(), &FGameProcess::HandleDiscoveryProgress);

		int32 AssetCnt = 0;
		for (const FAssetPoolDefinition& Pool : Pools)
		{
			if (!OwnerPtr.IsValid()) break;

			UE_LOG(LogTemp, Verbose, TEXT("AssetDiscoveryTask: Discovering pool %s"), *Pool.PoolName);
			AssetCnt += OwnerPtr->AssetDiscoverer->DiscoverAssetsInPool(Pool, DiscoveredDelegate);
			UE_LOG(LogTemp, Verbose, TEXT("AssetDiscoveryTask: Finished pool %s"), *Pool.PoolName);
		}
		OwnerPtr->SetTotalAssetCnt(AssetCnt);
		DiscoveredDelegate.ExecuteIfBound(nullptr);

		UE_LOG(LogTemp, Log, TEXT("AssetDiscoveryTask: Discovery work complete."));
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FAssetDiscoveryTask, STATGROUP_ThreadPoolAsyncTasks);
	}

private:
	TWeakPtr<FGameProcess> Owner;
};
