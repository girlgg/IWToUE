#pragma once

#include "CoreMinimal.h"

struct FXSubPackageCacheObject;
class IAsyncTaskQueue;
class IGameRepository;
class IFileMetaRepository;
class IXSubInfoRepository;
class IAssetNameRepository;
class IFileTracker;
class IDatabaseConnection;

DECLARE_MULTICAST_DELEGATE(FOnDatabaseInitializedDelegate);
DECLARE_MULTICAST_DELEGATE(FOnFileTrackingCompleteDelegate);

class FCoDDatabaseService
{
public:
	static FCoDDatabaseService& Get();

	void Initialize(const FString& PreferredDatabasePath = "");
	void Shutdown();
	bool IsInitialized() const { return bIsInitialized; }

	void SetCurrentGameContext(uint64 GameHash, const FString& GamePath);

	// --- Public Data Access API ---

	// AssetName Operations
	FString GetPrintfAssetName(uint64 Hash, const FString& NamePrefix = TEXT(""));
	TOptional<FString> GetAssetNameSync(uint64 Hash);
	void GetAssetNameAsync(uint64 Hash, TFunction<void(TOptional<FString>)> Callback);
	void UpdateAssetNameAsync(uint64 Hash, const FString& Value, TFunction<void(bool)> CompletionCallback = nullptr);
	void UpdateAssetNameBatchAsync(const TMap<uint64, FString>& Items,
	                               TFunction<void(bool)> CompletionCallback = nullptr);
	void DeleteAssetNameAsync(uint64 Hash, TFunction<void(bool)> CompletionCallback = nullptr);

	// XSub Operations
	TOptional<FXSubPackageCacheObject> GetXSubInfoSync(uint64 DecryptionKey);
	void GetXSubInfoAsync(uint64 DecryptionKey, TFunction<void(TOptional<FXSubPackageCacheObject>)> Callback);

	FOnDatabaseInitializedDelegate OnDatabaseInitialized;
	FOnFileTrackingCompleteDelegate OnFileTrackingComplete;

private:
	FCoDDatabaseService();
	~FCoDDatabaseService();

	FString GetDefaultDatabasePath() const;
	void HandleFileTrackingComplete();

	// 将任务推入延迟执行队列
	void EnqueueActualDbTask(TFunction<void()> Task);

	TSharedPtr<IDatabaseConnection> Connection;
	TSharedPtr<IAssetNameRepository> AssetNameRepo;
	TSharedPtr<IXSubInfoRepository> XSubInfoRepo;
	TSharedPtr<IFileMetaRepository> FileMetaRepo;
	TSharedPtr<IGameRepository> GameRepo;
	TSharedPtr<IAsyncTaskQueue> AsyncTaskQueue;
	TSharedPtr<IFileTracker> FileTracker;
	
	uint64 CurrentGameHash = 0;
	FString CurrentGamePath;

	FCriticalSection DeferredTasksLock;
	TQueue<TFunction<void()>> DeferredDbTasks;

	std::atomic<bool> bIsInitialized = false;
	std::atomic<bool> bIsFileTrackingComplete{false};
};
