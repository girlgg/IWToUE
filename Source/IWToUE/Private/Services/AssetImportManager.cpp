#include "Services/AssetImportManager.h"

#include "FileHelpers.h"
#include "SeLogChannels.h"
#include "GameInfo/GameAssetHandlerFactory.h"
#include "Importers/AnimationImporter.h"
#include "Importers/ImageImporter.h"
#include "Importers/MapImporter.h"
#include "Importers/MaterialImporter.h"
#include "Importers/ModelImporter.h"
#include "Importers/SoundImporter.h"
#include "WraithX/CoDAssetType.h"
#include "WraithX/GameProcess.h"

FAssetImportManager::FAssetImportManager()
{
	UE_LOG(LogITUAssetImportManager, Log, TEXT("FAssetImportManager Created."));
}

FAssetImportManager::~FAssetImportManager()
{
	Shutdown();
	UE_LOG(LogITUAssetImportManager, Log, TEXT("FAssetImportManager Destroyed."));
}

bool FAssetImportManager::Initialize()
{
	if (ProcessInstance.IsValid())
	{
		UE_LOG(LogITUAssetImportManager, Log, TEXT("AssetImportManager already initialized. Shutting down first."));
		Shutdown();
	}

	UE_LOG(LogITUAssetImportManager, Log, TEXT("Initializing AssetImportManager..."));

	ProcessInstance = MakeShared<FGameProcess>();
	if (!ProcessInstance->Initialize())
	{
		UE_LOG(LogITUAssetImportManager, Error,
		       TEXT("Failed to initialize FGameProcess. AssetImportManager initialization failed."));
		ProcessInstance.Reset();
		return false;
	}

	if (!UpdateGameHandler())
	{
		UE_LOG(LogITUAssetImportManager, Error,
		       TEXT("Failed to create a valid IGameAssetHandler. AssetImportManager initialization failed."));
		ProcessInstance.Reset();
		return false;
	}

	// Register importers
	SetupAssetImporters();

	// Bind to GameProcess delegates AFTER it's successfully initialized
	ProcessInstance->OnAssetLoadingProgress.AddSP(this, &FAssetImportManager::HandleDiscoveryProgressInternal);
	ProcessInstance->OnAssetLoadingComplete.AddSP(this, &FAssetImportManager::HandleDiscoveryCompleteInternal);

	UE_LOG(LogITUAssetImportManager, Log, TEXT("AssetImportManager Initialized Successfully."));
	return true;
}

void FAssetImportManager::Shutdown()
{
	UE_LOG(LogITUAssetImportManager, Log, TEXT("Shutting down AssetImportManager..."));

	// TODO: Add logic to cancel any running Async Task (CurrentImportTaskHandle)

	if (ProcessInstance.IsValid())
	{
		ProcessInstance->OnAssetLoadingProgress.RemoveAll(this);
		ProcessInstance->OnAssetLoadingComplete.RemoveAll(this);
		ProcessInstance.Reset();
	}

	CurrentGameHandler.Reset();
	AssetImporters.Empty();

	// Clear cache
	FScopeLock Lock(&CacheLock);
	ImportCache.Clear();

	bIsDiscoveringAssets = false;
	bIsImportingAssets = false;

	UE_LOG(LogITUAssetImportManager, Log, TEXT("AssetImportManager Shutdown Complete."));
}

void FAssetImportManager::StartAssetDiscovery()
{
	if (!ProcessInstance.IsValid())
	{
		UE_LOG(LogITUAssetImportManager, Error,
		       TEXT("Cannot start asset discovery: GameProcess is not initialized. Call Initialize() first."));
		return;
	}
	if (IsBusy())
	{
		UE_LOG(LogITUAssetImportManager, Warning,
		       TEXT("Cannot start asset discovery: Manager is busy (Discovering or Importing)."));
		return;
	}

	bIsDiscoveringAssets = true;
	OnDiscoveryProgressDelegate.Broadcast(0.0f);
	UE_LOG(LogITUAssetImportManager, Log, TEXT("Requesting asset discovery start via GameProcess..."));
	ProcessInstance->StartAssetDiscovery();
}

bool FAssetImportManager::IsBusy() const
{
	return bIsDiscoveringAssets || bIsImportingAssets;
}

TArray<TSharedPtr<FCoDAsset>> FAssetImportManager::GetDiscoveredAssets() const
{
	if (ProcessInstance)
	{
		return ProcessInstance->GetLoadedAssets();
	}
	return {};
}

bool FAssetImportManager::ImportAssetsAsync(const FString& BaseImportPath, TArray<TSharedPtr<FCoDAsset>> AssetsToImport,
                                            const FString& OptionalParams)
{
	if (!CurrentGameHandler.IsValid())
	{
		UE_LOG(LogITUAssetImportManager, Error,
		       TEXT("Cannot start import: No valid game handler. Ensure Initialize() was successful."));
		return false;
	}
	if (IsBusy())
	{
		UE_LOG(LogITUAssetImportManager, Warning, TEXT("Cannot start import: Manager is busy."));
		return false;
	}
	if (AssetsToImport.IsEmpty())
	{
		UE_LOG(LogITUAssetImportManager, Warning, TEXT("Cannot start import: No assets provided."));
		return true;
	}

	bIsImportingAssets = true;
	ImportCache.Clear();
	OnImportProgressDelegate.ExecuteIfBound(0.0f);

	UE_LOG(LogITUAssetImportManager, Log, TEXT("Starting asynchronous import of %d assets to %s..."),
	       AssetsToImport.Num(), *BaseImportPath);

	// Schedule the import task to run on a background thread pool
	Async(EAsyncExecution::ThreadPool,
	      [this, BaseImportPath, AssetsToImport = MoveTemp(AssetsToImport), OptionalParams]() mutable
	      {
		      RunImportTask(BaseImportPath, MoveTemp(AssetsToImport), OptionalParams);
	      });

	return true;
}

void FAssetImportManager::HandleDiscoveryCompleteInternal()
{
	AsyncTask(ENamedThreads::GameThread, [this]()
	{
		if (!bIsDiscoveringAssets) return;
		bIsDiscoveringAssets = false;
		UE_LOG(LogITUAssetImportManager, Log, TEXT("Received Asset Discovery Completion signal."));
		OnDiscoveryProgressDelegate.Broadcast(1.0f);
		OnDiscoveryCompletedDelegate.Broadcast();
	});
}

void FAssetImportManager::HandleDiscoveryProgressInternal(float Progress)
{
	AsyncTask(ENamedThreads::GameThread, [this, Progress]()
	{
		if (bIsDiscoveringAssets)
		{
			OnDiscoveryProgressDelegate.Broadcast(Progress);
		}
	});
}

void FAssetImportManager::RunImportTask(FString BaseImportPath, TArray<TSharedPtr<FCoDAsset>> AssetsToImport,
                                        FString OptionalParams)
{
	bool bOverallSuccess = true;
	int32 TotalAssets = AssetsToImport.Num();
	int32 CompletedAssets = 0;

	FText TaskLabel = FText::Format(
		NSLOCTEXT("AssetImportManager", "ImportTaskLabel", "Importing {0} Assets"), FText::AsNumber(TotalAssets));

	for (const TSharedPtr<FCoDAsset>& Asset : AssetsToImport)
	{
		if (!Asset.IsValid())
		{
			UE_LOG(LogITUAssetImportManager, Warning, TEXT("Skipping invalid asset entry in import list."));
			CompletedAssets++;
			continue;
		}

		// --- Progress Reporting ---
		float CurrentOverallProgress = (float)CompletedAssets / FMath::Max(1, TotalAssets);
		AsyncTask(ENamedThreads::GameThread, [this, CurrentOverallProgress]()
		{
			if (bIsImportingAssets)
			{
				OnImportProgressDelegate.ExecuteIfBound(CurrentOverallProgress);
			}
		});

		// --- Get Importer ---
		IAssetImporter* Importer = GetImporterForAsset(Asset->AssetType);
		if (!Importer)
		{
			UE_LOG(LogITUAssetImportManager, Warning,
			       TEXT("No importer registered for asset type %d (Asset: %s). Skipping."),
			       static_cast<int32>(Asset->AssetType), *Asset->AssetName);
			CompletedAssets++;
			continue;
		}

		// --- Prepare Context ---
		FAssetImportContext Context;
		Context.BaseImportPath = BaseImportPath;
		Context.OptionalParameters = OptionalParams;
		Context.SourceAsset = Asset;
		Context.GameHandler = CurrentGameHandler.Get();
		Context.ImportManager = this;
		Context.AssetCache = &ImportCache;

		TArray<UObject*> CreatedObjectsForThisAsset;
		FText AssetStatusText = FText::Format(
			NSLOCTEXT("AssetImportManager", "ImportStatusFormat", "Importing {0}..."),
			FText::FromString(Asset->AssetName));

		// --- Define Asset-Specific Progress Delegate ---
		FOnAssetImportProgress AssetProgressDelegate;
		AssetProgressDelegate.BindLambda(
			[this, CurrentOverallProgress, AssetName = Asset->AssetName, TotalAssets]
		(float AssetFraction, const FText& AssetStatus)
			{
				float UpdatedOverallProgress = CurrentOverallProgress + (AssetFraction / FMath::Max(1, TotalAssets));
				AsyncTask(ENamedThreads::GameThread, [this, UpdatedOverallProgress]()
				{
					if (bIsImportingAssets)
					{
						OnImportProgressDelegate.ExecuteIfBound(UpdatedOverallProgress);
					}
				});
			});


		// --- Execute Import ---
		UE_LOG(LogITUAssetImportManager, Log, TEXT("Importing asset '%s' (Type: %d) using %s..."),
		       *Asset->AssetName, (int32)Asset->AssetType,
		       Importer->GetSupportedUClass() ? *Importer->GetSupportedUClass()->GetName() : TEXT("Unknown UClass"));

		bool bAssetSuccess = Importer->Import(Context, CreatedObjectsForThisAsset, AssetProgressDelegate);

		// --- Handle Result ---
		if (!bAssetSuccess)
		{
			UE_LOG(LogITUAssetImportManager, Warning, TEXT("Failed to import asset: %s"), *Asset->AssetName);
			bOverallSuccess = false;
		}
		else
		{
			UE_LOG(LogITUAssetImportManager, Verbose, TEXT("Successfully imported asset: %s"), *Asset->AssetName);
		}

		CompletedAssets++;
	}

	if (bOverallSuccess)
	{
		AsyncTask(ENamedThreads::GameThread, [this]()
		{
			if (bIsImportingAssets)
			{
				SaveCachedAssets_GameThread();
			}
		});
	}
	else
	{
		UE_LOG(LogITUAssetImportManager, Warning,
		       TEXT("Import task finished with errors or was cancelled, skipping asset saving."));
	}

	AsyncTask(ENamedThreads::GameThread, [this, bOverallSuccess]()
	{
		bIsImportingAssets = false;
		UE_LOG(LogITUAssetImportManager, Log, TEXT("Asynchronous import task finished. Overall Success: %s"),
		       bOverallSuccess ? TEXT("True") : TEXT("False"));
		OnImportProgressDelegate.ExecuteIfBound(1.0f);
		OnImportCompletedDelegate.ExecuteIfBound(bOverallSuccess);
	});
}

void FAssetImportManager::SetupAssetImporters()
{
	UE_LOG(LogITUAssetImportManager, Log, TEXT("Setting up default asset importers..."));
	RegisterImporter(EWraithAssetType::Animation, MakeShared<FAnimationImporter>());
	RegisterImporter(EWraithAssetType::Image, MakeShared<FImageImporter>());
	RegisterImporter(EWraithAssetType::Model, MakeShared<FModelImporter>());
	RegisterImporter(EWraithAssetType::Sound, MakeShared<FSoundImporter>());
	RegisterImporter(EWraithAssetType::Material, MakeShared<FMaterialImporter>());
	RegisterImporter(EWraithAssetType::Map, MakeShared<FMapImporter>());
	UE_LOG(LogITUAssetImportManager, Log, TEXT("Registered %d asset importers."), AssetImporters.Num());
}

bool FAssetImportManager::UpdateGameHandler()
{
	if (!ProcessInstance.IsValid()) return false;
	CurrentGameHandler = FGameAssetHandlerFactory::CreateHandler(ProcessInstance);
	if (!CurrentGameHandler.IsValid())
	{
		UE_LOG(LogITUAssetImportManager, Error,
		       TEXT("GameAssetHandlerFactory failed to create a handler for the current game!"));
		return false;
	}
	UE_LOG(LogITUAssetImportManager, Log, TEXT("Game Asset Handler created/updated successfully."));
	return true;
}

void FAssetImportManager::RegisterImporter(EWraithAssetType Type, const TSharedPtr<IAssetImporter>& Importer)
{
	FScopeLock Lock(&ImporterMapLock);
	if (!Importer.IsValid())
	{
		UE_LOG(LogITUAssetImportManager, Warning, TEXT("Attempted to register an invalid importer for type %d"),
		       (int32)Type);
		return;
	}
	UE_LOG(LogITUAssetImportManager, Verbose, TEXT("Registering importer for type %d"), (int32)Type);
	AssetImporters.Add(Type, Importer);
}

IAssetImporter* FAssetImportManager::GetImporterForAsset(const EWraithAssetType Type)
{
	FScopeLock Lock(&ImporterMapLock);
	const TSharedPtr<IAssetImporter>* Found = AssetImporters.Find(Type);
	return Found && Found->IsValid() ? Found->Get() : nullptr;
}

void FAssetImportManager::SaveCachedAssets_GameThread()
{
	check(IsInGameThread());

	TArray<UPackage*> PackagesToSaveArray;
	{
		FScopeLock Lock(&CacheLock);
		if (ImportCache.PackagesToSave.IsEmpty())
		{
			UE_LOG(LogITUAssetImportManager, Log, TEXT("SaveCachedAssets_GameThread: No packages marked for saving."));
			return;
		}
		PackagesToSaveArray = ImportCache.PackagesToSave.Array();
		ImportCache.PackagesToSave.Empty();
	}

	UE_LOG(LogITUAssetImportManager, Log, TEXT("SaveCachedAssets_GameThread: Attempting to save %d packages..."),
	       PackagesToSaveArray.Num());

	bool bSaveSuccess = UEditorLoadingAndSavingUtils::SavePackages(PackagesToSaveArray, false);

	if (bSaveSuccess)
	{
		UE_LOG(LogITUAssetImportManager, Log, TEXT("Successfully saved %d packages via FEditorFileUtils."),
		       PackagesToSaveArray.Num());
	}
	else
	{
		UE_LOG(LogITUAssetImportManager, Error,
		       TEXT("FEditorFileUtils::SavePackages reported failure for one or more packages. Check log for details."
		       ));
	}
}
