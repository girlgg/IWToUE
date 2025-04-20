#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

class FGameProcess;
class IGameAssetHandler;
enum class EWraithAssetType : uint8;
class IAssetImporter;
struct FCoDAsset;

DECLARE_MULTICAST_DELEGATE(FOnAssetInitCompletedDelegate);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAssetLoadingDelegate, float);

class FAssetImportManager
{
public:
	FAssetImportManager();
	virtual ~FAssetImportManager() = default;

	//--- 初始化和刷新 ---

	// 连接进程，创建Handler
	void InitializeGame();
	// 重新加载游戏资产
	void RefreshGame();

	//--- 资产访问 ---

	// 获取已经加载的资产
	TArray<TSharedPtr<FCoDAsset>> GetLoadedAssets() const;

	//--- 导入 ---

	bool ImportSelection(FString ImportPath, TArray<TSharedPtr<FCoDAsset>> Selection, FString OptionalParams);

	FOnAssetInitCompletedDelegate OnAssetInitCompletedDelegate;
	FOnAssetLoadingDelegate OnAssetLoadingDelegate;

private:
	// --- 回调函数 ---
	
	void OnAssetInitCompletedInternal();
	void OnAssetLoadingInternal(float InProgress);
	
	void SetupAssetImporters();
	void UpdateGameHandler();

	TSharedPtr<FGameProcess> ProcessInstance;
	TSharedPtr<IGameAssetHandler> CurrentGameHandler;
	TMap<EWraithAssetType, TSharedPtr<IAssetImporter>> AssetImporters;
};
