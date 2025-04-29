#include "WraithX/GameProcess.h"

#include "SeLogChannels.h"
#include "CDN/CoDCDNDownloaderV2.h"
#include "GameInfo/GameAssetDiscovererFactory.h"
#include "Structures/MW6GameStructures.h"
#include "WraithX/LocateGameInfo.h"
#include "WraithX/WindowsMemoryReader.h"
#include "AssetImporter/AssetDiscoveryTask.h"

FGameProcess::FGameProcess()
{
}

FGameProcess::~FGameProcess()
{
	if (DiscoveryTask)
	{
		DiscoveryTask->EnsureCompletion();
		delete DiscoveryTask;
		DiscoveryTask = nullptr;
	}
	UE_LOG(LogITUMemoryReader, Log, TEXT("FGameProcess destroyed."));
}

bool FGameProcess::Initialize()
{
	if (bIsInitialized) return true;
	UE_LOG(LogITUMemoryReader, Log, TEXT("Initializing FGameProcess..."));
	if (!FindTargetProcess())
	{
		UE_LOG(LogITUMemoryReader, Error, TEXT("Initialization failed: Target game process not found."));
		return false;
	}
	if (!OpenProcessHandleAndReader())
	{
		UE_LOG(LogITUMemoryReader, Error,
		       TEXT("Initialization failed: Could not open process handle or create memory reader."));
		return false;
	}
	if (!LocateGameInfoViaParasyte())
	{
		UE_LOG(LogITUMemoryReader, Warning,
		       TEXT("Initialization warning: Failed to locate game info via Parasyte. Asset discovery might fail."));
	}
	if (!CreateAndInitializeDiscoverer())
	{
		UE_LOG(LogITUMemoryReader, Error,
		       TEXT("Initialization failed: Could not create or initialize a suitable game asset discoverer."));
		return false;
	}

	bIsInitialized = true;
	UE_LOG(LogITUMemoryReader, Log, TEXT("FGameProcess Initialized Successfully for %s (PID: %d)"),
	       *TargetProcessInfo.ProcessName,
	       TargetProcessId);
	return true;
}

bool FGameProcess::IsGameRunning()
{
	if (!TargetProcessId || !MemoryReader || !MemoryReader->IsValid())
	{
		return false;
	}
	HANDLE hProcessCheck = ::OpenProcess(SYNCHRONIZE, false, TargetProcessId);
	if (hProcessCheck == NULL)
	{
		return false;
	}
	DWORD Result = WaitForSingleObject(hProcessCheck, 0);
	CloseHandle(hProcessCheck);
	if (Result == WAIT_FAILED)
	{
		UE_LOG(LogITUMemoryReader, Warning, TEXT("IsGameRunning: WaitForSingleObject failed. Error: %d"),
		       GetLastError());
		return false;
	}
	return Result == WAIT_TIMEOUT;
}

void FGameProcess::StartAssetDiscovery()
{
	if (!bIsInitialized)
	{
		UE_LOG(LogITUMemoryReader, Error, TEXT("Cannot start asset discovery: FGameProcess not initialized."));
		return;
	}
	if (bIsDiscovering)
	{
		UE_LOG(LogITUMemoryReader, Warning, TEXT("Asset discovery already in progress."));
		return;
	}
	if (DiscoveryTask)
	{
		UE_LOG(LogITUMemoryReader, Warning, TEXT("Cleaning up previous discovery task before starting anew."));
		DiscoveryTask->EnsureCompletion();
		delete DiscoveryTask;
		DiscoveryTask = nullptr;
	}
	{
		FScopeLock Lock(&LoadedAssetsLock);
		LoadedAssets.Empty();
	}
	{
		FScopeLock Lock(&ProgressLock);
		CurrentDiscoveryProgressCount = 0.0f;
		TotalDiscoveredAssets = 0.0f;
	}
	bIsDiscovering = true;
	UE_LOG(LogITUMemoryReader, Log, TEXT("Starting asynchronous asset discovery..."));

	DiscoveryTask = new FAsyncTask<FAssetDiscoveryTask>(AsShared());
	DiscoveryTask->StartBackgroundTask();
}

bool FGameProcess::FindTargetProcess()
{
	TargetProcessId = 0;
	TargetProcessPath = "";
	bool bFound = false;

	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot != INVALID_HANDLE_VALUE)
	{
		PROCESSENTRY32 pe32;
		pe32.dwSize = sizeof(PROCESSENTRY32);
		if (Process32First(hSnapshot, &pe32))
		{
			do
			{
				FString CurrentProcessName = FString(pe32.szExeFile).ToLower();
				for (const auto& GameInfo : CoDAssets::GameProcessInfos)
				{
					if (CurrentProcessName == FString(*GameInfo.ProcessName).ToLower())
					{
						TargetProcessInfo = GameInfo;
						TargetProcessId = pe32.th32ProcessID;
						bFound = true;

						if (HANDLE hTempProcess = ::OpenProcess(
							PROCESS_QUERY_LIMITED_INFORMATION, false, TargetProcessId))
						{
							WCHAR path[MAX_PATH];
							if (GetModuleFileNameEx(hTempProcess, NULL, path, MAX_PATH))
							{
								TargetProcessPath = FString(path);
							}
							CloseHandle(hTempProcess);
							break;
						}
						UE_LOG(LogITUMemoryReader, Warning, TEXT("Could not open process %d to get path. Error: %d"),
						       TargetProcessId, GetLastError());
					}
				}
			}
			while (Process32Next(hSnapshot, &pe32) && !bFound);
		}
		CloseHandle(hSnapshot);
	}
	if (bFound)
		UE_LOG(LogITUMemoryReader, Log, TEXT("Found target process: %s (PID: %d) at %s"), *TargetProcessInfo.ProcessName,
	       TargetProcessId, *TargetProcessPath);
	return bFound;
}

bool FGameProcess::OpenProcessHandleAndReader()
{
	if (TargetProcessId == 0) return false;
	MemoryReader = MakeShared<FWindowsMemoryReader>(TargetProcessId);
	return MemoryReader->IsValid();
}

bool FGameProcess::LocateGameInfoViaParasyte()
{
	if (TargetProcessPath.IsEmpty() || !MemoryReader || !MemoryReader->IsValid()) return false;
	if (TargetProcessInfo.GameID == CoDAssets::ESupportedGames::Parasyte)
	{
		if (LocateGameInfo::Parasyte(TargetProcessPath, ParasyteState))
		{
			if (ParasyteState.IsValid())
			{
				UE_LOG(LogITUMemoryReader, Log, TEXT("Parasyte info located: GameID=0x%llX, Addr=0x%llX, Dir=%s"),
				       ParasyteState->GameID, ParasyteState->StringsAddress, *ParasyteState->GameDirectory);
				return true;
			}
			UE_LOG(LogITUMemoryReader, Warning,
			       TEXT("LocateGameInfo::Parasyte returned true but ParasyteState is null."));
			return false;
		}
	}
	UE_LOG(LogITUMemoryReader, Log, TEXT("Target process is not %s, But cannot open."),
	       *TargetProcessInfo.ProcessName);
	return false;
}

bool FGameProcess::CreateAndInitializeDiscoverer()
{
	if (!MemoryReader || !MemoryReader->IsValid()) return false;

	AssetDiscoverer = FGameAssetDiscovererFactory::CreateDiscoverer(TargetProcessInfo, ParasyteState);

	if (!AssetDiscoverer.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create an Asset Discoverer."));
		return false;
	}

	if (!AssetDiscoverer->Initialize(MemoryReader.Get(), TargetProcessInfo, ParasyteState))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to initialize the Asset Discoverer."));
		AssetDiscoverer.Reset();
		return false;
	}
	UE_LOG(LogITUMemoryReader, Log, TEXT("Asset Discoverer created and initialized successfully."));
	return true;
}

void FGameProcess::HandleAssetDiscovered(TSharedPtr<FCoDAsset> DiscoveredAsset)
{
	if (DiscoveredAsset.IsValid())
	{
		FScopeLock Lock(&LoadedAssetsLock);
		LoadedAssets.Add(MoveTemp(DiscoveredAsset));
	}
	if (TotalDiscoveredAssets != 0)
	{
		FScopeLock Lock(&ProgressLock);
		CurrentDiscoveryProgressCount = LoadedAssets.Num() / TotalDiscoveredAssets;
	}

	if (CurrentDiscoveryProgressCount > 0)
	{
		OnAssetLoadingProgress.Broadcast(CurrentDiscoveryProgressCount);
	}

	if (LoadedAssets.Num() == TotalDiscoveredAssets)
	{
		AsyncTask(ENamedThreads::GameThread, [this]()
		{
			HandleDiscoveryComplete();
		});
	}
}

void FGameProcess::HandleDiscoveryComplete()
{
	UE_LOG(LogTemp, Log, TEXT("Asset Discovery Complete. Total Assets Found: %d"), LoadedAssets.Num());
	bIsDiscovering = false;

	OnAssetLoadingProgress.Broadcast(1.0f);

	OnAssetLoadingComplete.Broadcast();

	if (DiscoveryTask)
	{
		// DiscoveryTask = nullptr;
	}
}

TSharedPtr<FXSub> FGameProcess::GetDecrypt()
{
	return AssetDiscoverer.IsValid() ? AssetDiscoverer->GetDecryptor() : nullptr;
}
