#include "Database/CoDDatabaseService.h"

#include "SeLogChannels.h"
#include "Database/CoDFileTracker.h"
#include "Database/DatabaseAsyncTaskQueue.h"
#include "Database/SqliteAssetNameRepository.h"
#include "Database/SQLiteConnection.h"
#include "Database/SqliteFileMetaRepository.h"
#include "Database/SqliteGameRepository.h"
#include "Database/SqliteXSubInfoRepository.h"
#include "Interface/IAsyncTaskQueue.h"
#include "Interface/IFileTracker.h"

FCoDDatabaseService& FCoDDatabaseService::Get()
{
	static FCoDDatabaseService Instance;
	return Instance;
}

void FCoDDatabaseService::Initialize(const FString& PreferredDatabasePath)
{
	if (bIsInitialized) return;
	UE_LOG(LogITUDatabase, Log, TEXT("Initializing CoD Database Service..."));

	bIsFileTrackingComplete = false;

	Connection = MakeShared<FSQLiteConnection>();
	if (const FString DBPath = PreferredDatabasePath.IsEmpty() ? GetDefaultDatabasePath() : PreferredDatabasePath;
		!Connection->Open(DBPath))
	{
		UE_LOG(LogITUDatabase, Fatal, TEXT("Database Service failed to initialize connection."));
		Connection.Reset();
		return;
	}

	AssetNameRepo = MakeShared<FSqliteAssetNameRepository>(Connection.ToSharedRef());
	XSubInfoRepo = MakeShared<FSqliteXSubInfoRepository>(Connection.ToSharedRef());
	FileMetaRepo = MakeShared<FSqliteFileMetaRepository>(Connection.ToSharedRef());
	GameRepo = MakeShared<FSqliteGameRepository>(Connection.ToSharedRef());

	AsyncTaskQueue = MakeShared<FDatabaseAsyncTaskQueue>();
	AsyncTaskQueue->TaskStart();

	FileTracker = MakeShared<FCoDFileTracker>(FileMetaRepo, GameRepo, AssetNameRepo, XSubInfoRepo);
	if (FileTracker)
	{
		FileTracker->OnComplete.BindRaw(this, &FCoDDatabaseService::HandleFileTrackingComplete);
	}

	bIsInitialized = true;
	UE_LOG(LogITUDatabase, Log, TEXT("CoD Database Service Initialized."));
	OnDatabaseInitialized.Broadcast();
}

void FCoDDatabaseService::Shutdown()
{
	if (!bIsInitialized) return;

	UE_LOG(LogTemp, Log, TEXT("Shutting down CoD Database Service..."));

	if (AsyncTaskQueue) AsyncTaskQueue->TaskStop();

	if (FileTracker)
	{
		FileTracker->OnComplete.Unbind();
		FileTracker.Reset();
	}
	if (AsyncTaskQueue)
	{
		AsyncTaskQueue->TaskStop();
		AsyncTaskQueue.Reset();
	}

	AssetNameRepo.Reset();
	XSubInfoRepo.Reset();
	FileMetaRepo.Reset();
	GameRepo.Reset();

	if (Connection)
	{
		Connection->Close();
		Connection.Reset();
	}

	// 清空延迟队列任务
	{
		FScopeLock Lock(&DeferredTasksLock);
		TFunction<void()> DiscardedTask;
		while (DeferredDbTasks.Dequeue(DiscardedTask))
		{
		}
	}

	bIsInitialized = false;
	bIsFileTrackingComplete = false;
	CurrentGameHash = 0;
	CurrentGamePath.Empty();

	UE_LOG(LogTemp, Log, TEXT("CoD Database Service Shutdown complete."));
}

void FCoDDatabaseService::SetCurrentGameContext(uint64 GameHash, const FString& GamePath)
{
	if (!bIsInitialized)
	{
		UE_LOG(LogTemp, Error, TEXT("Cannot set game context: Service not initialized."));
		return;
	}
	if (!FileTracker)
	{
		UE_LOG(LogITUDatabase, Error, TEXT("Cannot set game context: FileTracker is not available."));
		return;
	}
	// 如果上下文相同且已完成，无需重新跟踪
	if (CurrentGameHash == GameHash && CurrentGamePath == GamePath && bIsFileTrackingComplete.load())
	{
		UE_LOG(LogITUDatabase, Log,
		       TEXT("Database Service Context already set and file tracking complete for GameHash=%llu"), GameHash);
		return;
	}
	CurrentGameHash = GameHash;
	CurrentGamePath = GamePath;
	UE_LOG(LogTemp, Log, TEXT("Database Service Context Set: GameHash=%llu, Path=%s"), GameHash, *GamePath);

	// --- 重置状态并开始新的文件跟踪 ---
	bIsFileTrackingComplete = false;

	// 上下文变化，旧任务可能不再有效，清空可能存在的旧的延迟任务 
	{
		FScopeLock Lock(&DeferredTasksLock);
		int32 ClearedTasks = 0;
		TFunction<void()> DiscardedTask;
		while (DeferredDbTasks.Dequeue(DiscardedTask)) { ClearedTasks++; }
		if (ClearedTasks > 0)
		{
			UE_LOG(LogITUDatabase, Warning, TEXT("Cleared %d deferred DB tasks due to game context change."),
			       ClearedTasks);
		}
	}

	FileTracker->Initialize(CurrentGameHash, CurrentGamePath);
	FileTracker->StartTrackingAsync();
}

FString FCoDDatabaseService::GetPrintfAssetName(uint64 Hash, const FString& NamePrefix)
{
	Hash &= 0xFFFFFFFFFFFFFFF;
	TOptional<FString> QueryAssetName = GetAssetNameSync(Hash);
	if (QueryAssetName.IsSet())
		return QueryAssetName.GetValue();
	return FString::Printf(TEXT("%s_%llx"), *NamePrefix, Hash);
}

TOptional<FString> FCoDDatabaseService::GetAssetNameSync(uint64 Hash)
{
	if (!bIsInitialized || !AssetNameRepo) return TOptional<FString>();
	if (!bIsFileTrackingComplete.load())
	{
		UE_LOG(LogITUDatabase, Warning,
		       TEXT(
			       "GetAssetNameSync called before file tracking completed. Returning empty. Consider using Async version or waiting for OnFileTrackingComplete."
		       ));
		return TOptional<FString>();
	}
	return AssetNameRepo->QueryValue(Hash);
}

void FCoDDatabaseService::GetAssetNameAsync(uint64 Hash, TFunction<void(TOptional<FString>)> Callback)
{
	if (!bIsInitialized || !AsyncTaskQueue || !AssetNameRepo)
	{
		if (Callback)
			AsyncTask(ENamedThreads::GameThread, [Callback]() { Callback(TOptional<FString>()); });
		return;
	}

	TFunction<void()> DbTask = [Repo = AssetNameRepo, Hash, Callback]()
	{
		TOptional<FString> Result = Repo->QueryValue(Hash);
		AsyncTask(ENamedThreads::GameThread, [Callback, Result]()
		{
			if (Callback) Callback(Result);
		});
	};

	EnqueueActualDbTask(MoveTemp(DbTask));
}

void FCoDDatabaseService::UpdateAssetNameAsync(uint64 Hash, const FString& Value,
                                               TFunction<void(bool)> CompletionCallback)
{
	if (!bIsInitialized || !AsyncTaskQueue || !AssetNameRepo)
	{
		if (CompletionCallback)
			AsyncTask(ENamedThreads::GameThread, [CompletionCallback]()
			{
				CompletionCallback(false);
			});
		return;
	}
	TFunction<void()> DbTask = [Repo = AssetNameRepo, Hash, Value, CompletionCallback]()
	{
		bool bSuccess = Repo->InsertOrUpdate(Hash, Value);
		if (CompletionCallback)
		{
			AsyncTask(ENamedThreads::GameThread, [CompletionCallback, bSuccess]() { CompletionCallback(bSuccess); });
		}
	};

	EnqueueActualDbTask(MoveTemp(DbTask));
}

void FCoDDatabaseService::UpdateAssetNameBatchAsync(const TMap<uint64, FString>& Items,
                                                    TFunction<void(bool)> CompletionCallback)
{
	if (!bIsInitialized || !AsyncTaskQueue || !AssetNameRepo)
	{
		if (CompletionCallback)
			AsyncTask(ENamedThreads::GameThread, [CompletionCallback]()
			{
				CompletionCallback(false);
			});
		return;
	}
	TMap<uint64, FString> ItemsCopy = Items;
	TFunction<void()> DbTask = [Repo = AssetNameRepo, Items = MoveTemp(ItemsCopy), CompletionCallback]() mutable
	{
		bool bSuccess = Repo->BatchInsertOrUpdate(Items);
		if (CompletionCallback)
		{
			AsyncTask(ENamedThreads::GameThread, [CompletionCallback, bSuccess]() { CompletionCallback(bSuccess); });
		}
	};

	EnqueueActualDbTask(MoveTemp(DbTask));
}

void FCoDDatabaseService::DeleteAssetNameAsync(uint64 Hash, TFunction<void(bool)> CompletionCallback)
{
	if (!bIsInitialized || !AsyncTaskQueue || !AssetNameRepo)
	{
		if (CompletionCallback)
			AsyncTask(ENamedThreads::GameThread, [CompletionCallback]()
			{
				CompletionCallback(false);
			});
		return;
	}
	TFunction<void()> DbTask = [Repo = AssetNameRepo, Hash, CompletionCallback]()
	{
		bool bSuccess = Repo->DeleteByHash(Hash);
		if (CompletionCallback)
		{
			AsyncTask(ENamedThreads::GameThread, [CompletionCallback, bSuccess]() { CompletionCallback(bSuccess); });
		}
	};

	EnqueueActualDbTask(MoveTemp(DbTask));
}

TOptional<FXSubPackageCacheObject> FCoDDatabaseService::GetXSubInfoSync(uint64 DecryptionKey)
{
	if (!bIsInitialized || !XSubInfoRepo) return TOptional<FXSubPackageCacheObject>();
	if (!bIsFileTrackingComplete.load())
	{
		UE_LOG(LogITUDatabase, Warning,
		       TEXT(
			       "GetXSubInfoSync called before file tracking completed. Returning empty. Consider using Async version or waiting for OnFileTrackingComplete."
		       ));
		return TOptional<FXSubPackageCacheObject>();
	}
	return XSubInfoRepo->QueryValue(DecryptionKey);
}

void FCoDDatabaseService::GetXSubInfoAsync(uint64 DecryptionKey,
                                           TFunction<void(TOptional<FXSubPackageCacheObject>)> Callback)
{
	if (!bIsInitialized || !AsyncTaskQueue || !XSubInfoRepo)
	{
		if (Callback)
			AsyncTask(ENamedThreads::GameThread, [Callback]()
			{
				Callback(TOptional<FXSubPackageCacheObject>());
			});
		return;
	}

	TFunction<void()> DbTask = [Repo = XSubInfoRepo, DecryptionKey, Callback]()
	{
		TOptional<FXSubPackageCacheObject> Result = Repo->QueryValue(DecryptionKey);
		AsyncTask(ENamedThreads::GameThread, [Callback, Result]()
		{
			if (Callback) Callback(Result);
		});
	};

	EnqueueActualDbTask(MoveTemp(DbTask));
}

FCoDDatabaseService::FCoDDatabaseService()
{
	// 可以手动初始化，更可控，但这里暂时使用构造时自动初始化
	if (!bIsInitialized)
	{
		Initialize();
	}
}

FCoDDatabaseService::~FCoDDatabaseService()
{
	Shutdown();
}

FString FCoDDatabaseService::GetDefaultDatabasePath() const
{
	return FPaths::ProjectSavedDir() / TEXT("IWToUE") / "AssetCache.db";
}

void FCoDDatabaseService::HandleFileTrackingComplete()
{
	bool bCompExpected = false;
	if (bIsFileTrackingComplete.compare_exchange_strong(bCompExpected, true))
	{
		UE_LOG(LogITUDatabase, Log,
		       TEXT("File tracking completed. Database operations are now permitted. Processing deferred tasks..."));

		AsyncTask(ENamedThreads::GameThread, [this]()
		{
			OnFileTrackingComplete.Broadcast();
		});

		TFunction<void()> DeferredTask;
		while (true)
		{
			bool bDequeued = false;
			{
				FScopeLock Lock(&DeferredTasksLock);
				bDequeued = DeferredDbTasks.Dequeue(DeferredTask);
			}

			if (bDequeued)
			{
				AsyncTaskQueue->EnqueueTask(MoveTemp(DeferredTask));
			}
			else
			{
				break;
			}
		}
		UE_LOG(LogITUDatabase, Log, TEXT("Finished processing deferred DB tasks."));
	}
	else
	{
		UE_LOG(LogITUDatabase, Log,
		       TEXT("HandleFileTrackingComplete called, but tracking was already marked as complete."));
	}
}

void FCoDDatabaseService::EnqueueActualDbTask(TFunction<void()> Task)
{
	if (!bIsInitialized || !AsyncTaskQueue)
	{
		UE_LOG(LogITUDatabase, Error,
		       TEXT("Cannot enqueue DB task: Service not initialized or AsyncTaskQueue is null."));
		return;
	}

	if (bIsFileTrackingComplete.load())
	{
		// 文件跟踪已完成，直接加入DB任务队列
		AsyncTaskQueue->EnqueueTask(MoveTemp(Task));
	}
	else
	{
		// 文件跟踪未完成，加入延迟队列
		UE_LOG(LogITUDatabase, Verbose, TEXT("Deferring DB task until file tracking is complete."));
		FScopeLock Lock(&DeferredTasksLock);
		DeferredDbTasks.Enqueue(MoveTemp(Task));
	}
}
