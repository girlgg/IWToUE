#include "WraithX/GameProcess.h"

#include "SeLogChannels.h"
#include "CDN/CoDCDNDownloaderV2.h"
#include "GameInfo/GameAssetDiscovererFactory.h"
#include "Structures/MW6GameStructures.h"
#include "WraithX/LocateGameInfo.h"
#include "WraithX/WindowsMemoryReader.h"

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
	// MemoryReader is now responsible for opening/closing the handle
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

bool FGameProcess::LocateGameInfo()
{
	switch (ProcessInfo.GameID)
	{
	case CoDAssets::ESupportedGames::Parasyte:
		return LocateGameInfo::Parasyte(ProcessPath, ParasyteBaseState);
	default:
		break;
	}
	return false;
}

CoDAssets::ESupportedGames FGameProcess::GetCurrentGameType() const
{
	return AssetDiscoverer.IsValid() ? AssetDiscoverer->GetGameType() : CoDAssets::ESupportedGames::None;
}

CoDAssets::ESupportedGameFlags FGameProcess::GetCurrentGameFlag() const
{
	return AssetDiscoverer.IsValid() ? AssetDiscoverer->GetGameFlags() : CoDAssets::ESupportedGameFlags::None;
}

FCoDCDNDownloader* FGameProcess::GetCDNDownloader()
{
	return AssetDiscoverer.IsValid() ? AssetDiscoverer->GetCDNDownloader() : nullptr;
}

TSharedPtr<FXSub> FGameProcess::GetDecrypt()
{
	return AssetDiscoverer.IsValid() ? AssetDiscoverer->GetDecryptor() : nullptr;
}

void FGameProcess::ProcessModelAsset(FXAsset64 AssetNode)
{
	/*FMW6XModel Model = ReadMemory<FMW6XModel>(AssetNode.Header);
	Model.Hash &= 0xFFFFFFFFFFFFFFF;

	auto CreateModel = [&](const FString& ModelName)
	{
		TSharedPtr<FCoDModel> LoadedModel = MakeShared<FCoDModel>();
		LoadedModel->AssetType = EWraithAssetType::Model;
		LoadedModel->AssetName = ModelName;
		LoadedModel->AssetPointer = AssetNode.Header;
		LoadedModel->BoneCount = (Model.ParentListPtr > 0) * (Model.NumBones + Model.UnkBoneCount);
		LoadedModel->LodCount = Model.NumLods;
		LoadedModel->AssetStatus = AssetNode.Temp == 1
			                           ? EWraithAssetStatus::Placeholder
			                           : EWraithAssetStatus::Loaded;

		AddAssetToCollection(LoadedModel);
	};

	if (Model.NamePtr)
	{
		CreateModel(ProcessAssetName(ReadFString(Model.NamePtr)));
	}
	else
	{
		FCoDAssetDatabase::Get().QueryValueAsync(Model.Hash, [=](const FString& QueryName)
		{
			CreateModel(QueryName.IsEmpty()
				            ? FString::Printf(TEXT("xmodel_%llx"), Model.Hash)
				            : QueryName);
		});
	}*/
}

void FGameProcess::ProcessImageAsset(FXAsset64 AssetNode)
{
	// ProcessGenericAsset<FMW6GfxImage, FCoDImage>(AssetNode, TEXT("ximage"),
	//                                              [](auto& Image, auto LoadedImage)
	//                                              {
	// 	                                             LoadedImage->AssetType = EWraithAssetType::Image;
	// 	                                             LoadedImage->Width = Image.Width;
	// 	                                             LoadedImage->Height = Image.Height;
	// 	                                             LoadedImage->Format = Image.ImageFormat;
	// 	                                             LoadedImage->Streamed = Image.LoadedImagePtr == 0;
	//                                              });
}

void FGameProcess::ProcessAnimAsset(FXAsset64 AssetNode)
{
	// ProcessGenericAsset<FMW6XAnim, FCoDAnim>(AssetNode, TEXT("xanim"),
	//                                          [](auto& Anim, auto LoadedAnim)
	//                                          {
	// 	                                         LoadedAnim->AssetType = EWraithAssetType::Animation;
	// 	                                         LoadedAnim->Framerate = Anim.Framerate;
	// 	                                         LoadedAnim->FrameCount = Anim.FrameCount;
	// 	                                         LoadedAnim->BoneCount = Anim.TotalBoneCount;
	//                                          });
}

void FGameProcess::ProcessMaterialAsset(FXAsset64 AssetNode)
{
	// ProcessGenericAsset<FMW6Material, FCoDMaterial>(AssetNode, TEXT("xmaterial"),
	//                                                 [](auto& Material, auto LoadedMaterial)
	//                                                 {
	// 	                                                LoadedMaterial->AssetType = EWraithAssetType::Material;
	// 	                                                LoadedMaterial->ImageCount = Material.ImageCount;
	//                                                 });
}

void FGameProcess::ProcessSoundAsset(FXAsset64 AssetNode)
{
	// ProcessGenericAsset<FMW6SoundAsset, FCoDSound>(AssetNode, TEXT("xmaterial"),
	//                                                [](auto& SoundAsset, auto LoadedSound)
	//                                                {
	// 	                                               LoadedSound->FullName = LoadedSound->AssetName;
	// 	                                               LoadedSound->AssetType = EWraithAssetType::Sound;
	// 	                                               LoadedSound->AssetName = FPaths::GetBaseFilename(
	// 		                                               LoadedSound->AssetName);
	// 	                                               int32 DotIndex = LoadedSound->AssetName.Find(TEXT("."));
	// 	                                               if (DotIndex != INDEX_NONE)
	// 	                                               {
	// 		                                               LoadedSound->AssetName = LoadedSound->AssetName.Left(
	// 			                                               DotIndex);
	// 	                                               }
	// 	                                               LoadedSound->FullPath = FPaths::GetPath(LoadedSound->AssetName);
	//
	// 	                                               LoadedSound->FrameRate = SoundAsset.FrameRate;
	// 	                                               LoadedSound->FrameCount = SoundAsset.FrameCount;
	// 	                                               LoadedSound->ChannelsCount = SoundAsset.ChannelCount;
	// 	                                               LoadedSound->AssetSize = -1;
	// 	                                               LoadedSound->bIsFileEntry = false;
	// 	                                               LoadedSound->Length = 1000.0f * (LoadedSound->FrameCount /
	// 		                                               static_cast<float>(LoadedSound->FrameRate));
	//                                                });
}
