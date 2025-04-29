#include "WraithX/WraithXViewModel.h"

#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "SeLogChannels.h"
#include "Services/AssetImportManager.h"
#include "WraithX/CoDAssetType.h"

FName EWraithXSortColumnToFName(EWraithXSortColumn Column)
{
	switch (Column)
	{
	case EWraithXSortColumn::AssetName: return FName("AssetName");
	case EWraithXSortColumn::Status: return FName("Status");
	case EWraithXSortColumn::Type: return FName("Type");
	case EWraithXSortColumn::Size: return FName("Size");
	default: return NAME_None;
	}
}

EWraithXSortColumn FNameToEWraithXSortColumn(FName Name)
{
	if (Name == FName("AssetName")) return EWraithXSortColumn::AssetName;
	if (Name == FName("Status")) return EWraithXSortColumn::Status;
	if (Name == FName("Type")) return EWraithXSortColumn::Type;
	if (Name == FName("Size")) return EWraithXSortColumn::Size;
	return EWraithXSortColumn::None;
}

FWraithXViewModel::FWraithXViewModel()
{
	ImportPath = TEXT("/Game/ImportedAssets/");
	UE_LOG(LogITUAssetImporter, Log, TEXT("FWraithXViewModel Created."));
}

FWraithXViewModel::~FWraithXViewModel()
{
	Cleanup();
	UE_LOG(LogITUAssetImporter, Log, TEXT("FWraithXViewModel Destroyed."));
}

void FWraithXViewModel::Initialize(const TSharedPtr<FAssetImportManager>& InAssetImportManager)
{
	if (!InAssetImportManager.IsValid())
	{
		UE_LOG(LogITUAssetImporter, Error,
		       TEXT("FWraithXViewModel::Initialize - Invalid FAssetImportManager provided!"));
		ShowNotification(NSLOCTEXT("WraithXViewModel", "InitError", "Initialization Failed: Asset Manager Invalid."));
		return;
	}

	Cleanup();

	AssetImportManager = InAssetImportManager;

	AssetImportManager->OnDiscoveryCompletedDelegate.AddSP(this, &FWraithXViewModel::HandleDiscoveryCompleted);
	AssetImportManager->OnDiscoveryProgressDelegate.AddSP(this, &FWraithXViewModel::HandleDiscoveryProgress);
	AssetImportManager->OnImportCompletedDelegate.BindSP(this, &FWraithXViewModel::HandleImportCompleted);
	AssetImportManager->OnImportProgressDelegate.BindSP(this, &FWraithXViewModel::HandleImportProgress);

	UE_LOG(LogITUAssetImporter, Log, TEXT("ViewModel Initialized and bound to AssetImportManager delegates."));
}

void FWraithXViewModel::Cleanup()
{
	UE_LOG(LogITUAssetImporter, Log, TEXT("Cleaning up ViewModel..."));
	CancelCurrentFilterSortTask();

	if (AssetImportManager.IsValid())
	{
		AssetImportManager->OnDiscoveryCompletedDelegate.RemoveAll(this);
		AssetImportManager->OnDiscoveryProgressDelegate.RemoveAll(this);
		AssetImportManager->OnImportCompletedDelegate.Unbind();
		AssetImportManager->OnImportProgressDelegate.Unbind();
		AssetImportManager.Reset();
	}

	{
		FScopeLock Lock(&DataLock);
		AllDiscoveredAssets.Empty();
		FilteredItems.Empty();
		SelectedItems.Empty();
	}
	TotalAssetCount = 0;
	CurrentSearchText.Empty();
	OptionalParams.Empty();
	CurrentSortColumn = EWraithXSortColumn::None;
	CurrentSortMode = EColumnSortMode::None;

	SetBusyState(false);
	ExecuteDelegateOnGameThread(OnListChangedDelegate);
	ExecuteDelegateOnGameThread(OnAssetCountChangedDelegate, TotalAssetCount.load(), 0);
	UE_LOG(LogITUAssetImporter, Log, TEXT("ViewModel Cleanup Complete."));
}

const TArray<TSharedPtr<FCoDAsset>>& FWraithXViewModel::GetFilteredItems() const
{
	return FilteredItems;
}

const TArray<TSharedPtr<FCoDAsset>>& FWraithXViewModel::GetSelectedItems() const
{
	return SelectedItems;
}

float FWraithXViewModel::GetCurrentProgress() const
{
	return CurrentProgressValue;
}

bool FWraithXViewModel::IsBusy() const
{
	return bIsBusyFlag.load();
}

int32 FWraithXViewModel::GetTotalAssetCount() const
{
	return TotalAssetCount.load();
}

int32 FWraithXViewModel::GetFilteredAssetCount()
{
	FScopeLock Lock(&DataLock);
	return FilteredItems.Num();
}

const FString& FWraithXViewModel::GetImportPath() const
{
	return ImportPath;
}

const FString& FWraithXViewModel::GetOptionalParams() const
{
	return OptionalParams;
}

EColumnSortMode::Type FWraithXViewModel::GetSortModeForColumn(FName ColumnId) const
{
	return (CurrentSortColumn == FNameToEWraithXSortColumn(ColumnId)) ? CurrentSortMode : EColumnSortMode::None;
}

const FString& FWraithXViewModel::GetSearchText() const
{
	return CurrentSearchText;
}

void FWraithXViewModel::SetSearchText(const FString& InText)
{
	if (FString TrimmedText = InText.TrimStartAndEnd(); CurrentSearchText != TrimmedText)
	{
		CurrentSearchText = TrimmedText;
		ApplyFilteringAndSortingAsync();
	}
}

void FWraithXViewModel::ClearSearchText()
{
	if (!CurrentSearchText.IsEmpty())
	{
		CurrentSearchText.Empty();
		ApplyFilteringAndSortingAsync();
	}
}

void FWraithXViewModel::StartDiscovery()
{
	if (!AssetImportManager.IsValid())
	{
		ShowNotification(NSLOCTEXT("WraithXViewModel", "ErrorNoManager", "Asset Manager not available."));
		return;
	}
	if (IsBusy())
	{
		ShowNotification(NSLOCTEXT("WraithXViewModel", "WarningBusy", "Operation already in progress."));
		return;
	}

	UE_LOG(LogITUAssetImporter, Log, TEXT("StartDiscovery command received."));
	SetBusyState(true);
	UpdateProgress(0.0f);

	{
		FScopeLock Lock(&DataLock);
		AllDiscoveredAssets.Empty();
		FilteredItems.Empty();
		SelectedItems.Empty();
	}

	TotalAssetCount = 0;
	ExecuteDelegateOnGameThread(OnListChangedDelegate);
	ExecuteDelegateOnGameThread(OnAssetCountChangedDelegate, 0, 0);

	ShowNotification(NSLOCTEXT("WraithXViewModel", "LoadingGame", "Loading game assets..."));
	AssetImportManager->StartAssetDiscovery();
}

void FWraithXViewModel::ImportSelectedAssets()
{
	if (!AssetImportManager.IsValid())
	{
		ShowNotification(NSLOCTEXT("WraithXViewModel", "ErrorNoManager", "Asset Manager not available."));
		return;
	}
	if (IsBusy())
	{
		ShowNotification(NSLOCTEXT("WraithXViewModel", "WarningBusy", "Operation already in progress."));
		return;
	}
	if (SelectedItems.IsEmpty())
	{
		ShowNotification(NSLOCTEXT("WraithXViewModel", "NoAssetsSelected", "No assets selected for import."));
		return;
	}
	if (ImportPath.IsEmpty() || !ImportPath.StartsWith(TEXT("/Game/")))
	{
		ShowNotification(NSLOCTEXT("WraithXViewModel", "InvalidImportPath",
		                           "Please set a valid import path starting with /Game/."));
		return;
	}

	UE_LOG(LogITUAssetImporter, Log, TEXT("ImportSelectedAssets command received for %d items."), SelectedItems.Num());

	SetBusyState(true);
	UpdateProgress(0.0f);
	ShowNotification(FText::Format(
		NSLOCTEXT("WraithXViewModel", "ImportStartedSelected", "Starting import for {0} selected assets to {1}..."),
		FText::AsNumber(SelectedItems.Num()), FText::FromString(ImportPath)));

	const TArray<TSharedPtr<FCoDAsset>> ItemsToImport = SelectedItems;
	AssetImportManager->ImportAssetsAsync(ImportPath, ItemsToImport, OptionalParams);
}

void FWraithXViewModel::ImportAllFilteredAssets()
{
	if (!AssetImportManager.IsValid())
	{
		ShowNotification(NSLOCTEXT("WraithXViewModel", "ErrorNoManager", "Asset Manager not available."));
		return;
	}
	if (IsBusy())
	{
		ShowNotification(NSLOCTEXT("WraithXViewModel", "WarningBusy", "Operation already in progress."));
		return;
	}

	TArray<TSharedPtr<FCoDAsset>> CurrentFilteredItems;
	{
		FScopeLock Lock(&DataLock); // Access FilteredItems safely
		CurrentFilteredItems = FilteredItems;
	}

	if (CurrentFilteredItems.IsEmpty())
	{
		if (AllDiscoveredAssets.IsEmpty())
		{
			ShowNotification(NSLOCTEXT("WraithXViewModel", "NoAssetsToImport", "Load assets before importing."));
		}
		else
		{
			ShowNotification(NSLOCTEXT("WraithXViewModel", "NoAssetsFiltered",
			                           "No assets match the current filter for import."));
		}
		return;
	}
	if (ImportPath.IsEmpty() || !ImportPath.StartsWith(TEXT("/Game/")))
	{
		ShowNotification(NSLOCTEXT("WraithXViewModel", "InvalidImportPath",
		                           "Please set a valid import path starting with /Game/."));
		return;
	}

	UE_LOG(LogITUAssetImporter, Log, TEXT("ImportAllFilteredAssets command received for %d items."),
	       CurrentFilteredItems.Num());

	SetBusyState(true);
	UpdateProgress(0.0f);
	ShowNotification(FText::Format(
		NSLOCTEXT("WraithXViewModel", "ImportStartedAll", "Starting import for {0} filtered assets to {1}..."),
		FText::AsNumber(CurrentFilteredItems.Num()), FText::FromString(ImportPath)));

	AssetImportManager->ImportAssetsAsync(ImportPath, CurrentFilteredItems, OptionalParams);
}

void FWraithXViewModel::SetImportPath(const FString& Path)
{
	FString SanitizedPath = Path.TrimStartAndEnd();
	SanitizedPath.ReplaceInline(TEXT("\\"), TEXT("/"));

	if (!SanitizedPath.IsEmpty() && !SanitizedPath.StartsWith(TEXT("/Game/")))
	{
		ShowNotification(NSLOCTEXT("WraithXViewModel", "PathErrorHint",
		                           "Import path must start with /Game/ and be inside the Content directory."));
	}

	FPaths::NormalizeDirectoryName(SanitizedPath);
	if (!SanitizedPath.EndsWith(TEXT("/")))
	{
		SanitizedPath += TEXT("/");
	}

	if (ImportPath != SanitizedPath)
	{
		ImportPath = SanitizedPath;
		ExecuteDelegateOnGameThread(OnImportPathChangedDelegate, ImportPath);
	}
}

void FWraithXViewModel::SetOptionalParams(const FString& Params)
{
	if (FString TrimmedParams = Params.TrimStartAndEnd(); OptionalParams != TrimmedParams)
	{
		OptionalParams = TrimmedParams;
	}
}

void FWraithXViewModel::BrowseImportPath(const TSharedPtr<SWindow>& ParentWindow)
{
	if (IsBusy()) return;

	if (IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get())
	{
		const void* ParentWindowHandle = ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid()
			                                 ? ParentWindow->GetNativeWindow()->GetOSWindowHandle()
			                                 : nullptr;

		const FString DefaultPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
		const FString Title = NSLOCTEXT("WraithXViewModel", "BrowseDialogTitle",
		                                "Select Import Directory (/Content Folder)").ToString();

		if (FString SelectedPath;
			DesktopPlatform->OpenDirectoryDialog(ParentWindowHandle, Title, DefaultPath, SelectedPath))
		{
			if (FString RelativePath = SelectedPath;
				FPaths::MakePathRelativeTo(RelativePath, *FPaths::ProjectContentDir()))
			{
				RelativePath = FString(TEXT("/Game/")) + RelativePath;
				SetImportPath(RelativePath);
			}
			else
			{
				ShowNotification(NSLOCTEXT("WraithXViewModel", "BrowseErrorHint",
				                           "Selected path must be within the project's Content directory."));
			}
		}
	}
	else
	{
		ShowNotification(NSLOCTEXT("WraithXViewModel", "DesktopPlatformError",
		                           "Cannot open directory browser. DesktopPlatform module not available."));
	}
}

void FWraithXViewModel::SetSortColumn(FName ColumnId)
{
	EWraithXSortColumn NewSortColumn = FNameToEWraithXSortColumn(ColumnId);
	if (NewSortColumn == EWraithXSortColumn::None) return;

	if (CurrentSortColumn != NewSortColumn)
	{
		CurrentSortColumn = NewSortColumn;
		CurrentSortMode = EColumnSortMode::Ascending;
	}
	else
	{
		CurrentSortMode = (CurrentSortMode == EColumnSortMode::Ascending)
			                  ? EColumnSortMode::Descending
			                  : EColumnSortMode::Ascending;
	}

	ApplyFilteringAndSortingAsync();
}

void FWraithXViewModel::UpdateSelection(const TArray<TSharedPtr<FCoDAsset>>& InSelectedItems)
{
	SelectedItems = InSelectedItems;
}

void FWraithXViewModel::RequestPreviewAsset(const TSharedPtr<FCoDAsset>& Item)
{
	if (Item.IsValid())
	{
		ExecuteDelegateOnGameThread(OnPreviewAssetDelegate, Item);
	}
}

void FWraithXViewModel::SetBusyState(bool bNewBusyState)
{
	bool bCurrentBusy = bIsBusyFlag.load();
	if (bCurrentBusy != bNewBusyState)
	{
		bIsBusyFlag.store(bNewBusyState);
		UE_LOG(LogITUAssetImporter, Log, TEXT("Setting Busy State to: %s"),
		       bNewBusyState ? TEXT("true") : TEXT("false"));
		ExecuteDelegateOnGameThread(OnLoadingStateChangedDelegate, bNewBusyState);

		UpdateProgress(bNewBusyState ? 0.0f : 1.0f);
	}
}

void FWraithXViewModel::UpdateProgress(float InProgress)
{
	float ClampedProgress = FMath::Clamp(InProgress, 0.0f, 1.0f);
	CurrentProgressValue.store(ClampedProgress);
	ExecuteDelegateOnGameThread(OnLoadingProgressChangedDelegate, ClampedProgress);
}

void FWraithXViewModel::UpdateAssetLists(TArray<TSharedPtr<FCoDAsset>> NewFilteredItems, int32 NewTotalCount)
{
	check(IsInGameThread());

	{
		FScopeLock Lock(&DataLock);
		FilteredItems = MoveTemp(NewFilteredItems);
	}
	TotalAssetCount.store(NewTotalCount);

	TArray<TSharedPtr<FCoDAsset>> ValidSelectedItems;
	if (!SelectedItems.IsEmpty())
	{
		FScopeLock Lock(&DataLock);
		for (const auto& Selected : SelectedItems)
		{
			if (FilteredItems.Contains(Selected))
			{
				ValidSelectedItems.Add(Selected);
			}
		}
	}
	SelectedItems = MoveTemp(ValidSelectedItems);

	OnListChangedDelegate.ExecuteIfBound();
	OnAssetCountChangedDelegate.ExecuteIfBound(TotalAssetCount.load(), GetFilteredAssetCount());
}

void FWraithXViewModel::HandleDiscoveryCompleted()
{
	UE_LOG(LogITUAssetImporter, Log, TEXT("HandleDiscoveryCompleted received."));

	if (AssetImportManager.IsValid())
	{
		FScopeLock Lock(&DataLock);
		AllDiscoveredAssets = AssetImportManager->GetDiscoveredAssets();
	}
	else
	{
		FScopeLock Lock(&DataLock);
		AllDiscoveredAssets.Empty();
	}

	ApplyFilteringAndSortingAsync();

	SetBusyState(false);
	ShowNotification(FText::Format(
		NSLOCTEXT("WraithXViewModel", "LoadComplete", "Asset discovery complete. Found {0} assets."),
		FText::AsNumber(AllDiscoveredAssets.Num())));
}

void FWraithXViewModel::HandleDiscoveryProgress(float InProgress)
{
	if (IsBusy())
	{
		UpdateProgress(InProgress * 0.99f);
	}
}

void FWraithXViewModel::HandleImportCompleted(const bool bSuccess)
{
	UE_LOG(LogITUAssetImporter, Log, TEXT("HandleImportCompleted received. Success: %s"),
	       bSuccess ? TEXT("true"): TEXT("false"));
	SetBusyState(false);

	if (bSuccess)
	{
		ShowNotification(NSLOCTEXT("WraithXViewModel", "ImportCompleteSuccess",
		                           "Import process completed successfully."));
	}
	else
	{
		ShowNotification(NSLOCTEXT("WraithXViewModel", "ImportCompleteFail",
		                           "Import process finished with errors. Check log."));
	}
}

void FWraithXViewModel::HandleImportProgress(float Progress)
{
	if (IsBusy())
	{
		UpdateProgress(Progress);
	}
}

void FWraithXViewModel::ApplyFilteringAndSortingAsync()
{
	CancelCurrentFilterSortTask();

	FString SearchTextCopy = CurrentSearchText;
	TArray<TSharedPtr<FCoDAsset>> AssetsToProcess;
	{
		FScopeLock Lock(&DataLock);
		AssetsToProcess = AllDiscoveredAssets;
	}
	int32 CurrentTotalCount = AssetsToProcess.Num();

	CurrentFilterSortTask = Async(EAsyncExecution::ThreadPool,
	                              [this, AssetsToProcess, SearchTextCopy ]() mutable -> TArray<TSharedPtr<FCoDAsset>>
	                              {
		                              TArray<TSharedPtr<FCoDAsset>> TempFilteredItems = FilterAssets_WorkerThread(
			                              AssetsToProcess, SearchTextCopy);

		                              SortFilteredItems_Internal(TempFilteredItems);

		                              return TempFilteredItems;
	                              });

	CurrentFilterSortTask.Then([this, CurrentTotalCount](const TFuture<TArray<TSharedPtr<FCoDAsset>>>& ResultFuture)
	{
		if (!this) return;

		TArray<TSharedPtr<FCoDAsset>> NewFilteredItems;

		if (ResultFuture.IsReady())
		{
			NewFilteredItems = ResultFuture.Get();
		}
		else
		{
			UE_LOG(LogITUAssetImporter, Warning, TEXT("Filtering/Sorting task did not complete successfully."));
			return;
		}

		AsyncTask(ENamedThreads::GameThread,
		          [this, Items = MoveTemp(NewFilteredItems), CurrentTotalCount]() mutable
		          {
			          if (this)
			          {
				          UpdateAssetLists(MoveTemp(Items), CurrentTotalCount);
			          }
		          });
	});
}

void FWraithXViewModel::CancelCurrentFilterSortTask()
{
	if (CurrentFilterSortTask.IsValid())
	{
		CurrentFilterSortTask.Reset();
	}
}

TArray<TSharedPtr<FCoDAsset>> FWraithXViewModel::FilterAssets_WorkerThread(
	const TArray<TSharedPtr<FCoDAsset>>& AssetsToFilter, const FString& SearchText)
{
	if (SearchText.IsEmpty())
	{
		return AssetsToFilter;
	}
	TArray<TSharedPtr<FCoDAsset>> Result;
	Result.Reserve(AssetsToFilter.Num() / 2);

	TArray<FString> SearchTokens;
	SearchText.ParseIntoArray(SearchTokens, TEXT(" "), true);

	for (const auto& Item : AssetsToFilter)
	{
		if (Item.IsValid() && MatchSearchCondition(Item, SearchTokens))
		{
			Result.Add(Item);
		}
	}

	return Result;
}

void FWraithXViewModel::SortFilteredItems_Internal(TArray<TSharedPtr<FCoDAsset>>& ItemsToSort) const
{
	if (CurrentSortColumn == EWraithXSortColumn::None || ItemsToSort.Num() < 2)
	{
		return;
	}

	bool bAscending = CurrentSortMode == EColumnSortMode::Ascending;

	auto CompareNames = [bAscending](const TSharedPtr<FCoDAsset>& A, const TSharedPtr<FCoDAsset>& B)
	{
		if (!A.IsValid()) return !bAscending;
		if (!B.IsValid()) return bAscending;
		const int32 CompareResult = A->AssetName.Compare(B->AssetName, ESearchCase::IgnoreCase);
		return bAscending ? (CompareResult < 0) : (CompareResult > 0);
	};

	auto CompareStatus = [bAscending](const TSharedPtr<FCoDAsset>& A, const TSharedPtr<FCoDAsset>& B)
	{
		if (!A.IsValid()) return !bAscending;
		if (!B.IsValid()) return bAscending;
		const int32 CompareResult = A->GetAssetStatusText().ToString().Compare(
			B->GetAssetStatusText().ToString(), ESearchCase::IgnoreCase);
		return bAscending ? (CompareResult < 0) : (CompareResult > 0);
	};

	auto CompareType = [bAscending](const TSharedPtr<FCoDAsset>& A, const TSharedPtr<FCoDAsset>& B)
	{
		if (!A.IsValid()) return !bAscending;
		if (!B.IsValid()) return bAscending;
		int32 CompareResult = A->GetAssetTypeText().ToString().Compare(B->GetAssetTypeText().ToString(),
		                                                               ESearchCase::IgnoreCase);
		return bAscending ? (CompareResult < 0) : (CompareResult > 0);
	};

	auto CompareSize = [bAscending](const TSharedPtr<FCoDAsset>& A, const TSharedPtr<FCoDAsset>& B)
	{
		if (!A.IsValid()) return !bAscending;
		if (!B.IsValid()) return bAscending;
		if (A->AssetSize == B->AssetSize) return false;
		return bAscending ? (A->AssetSize < B->AssetSize) : (A->AssetSize > B->AssetSize);
	};

	switch (CurrentSortColumn)
	{
	case EWraithXSortColumn::AssetName: ItemsToSort.Sort(CompareNames);
		break;
	case EWraithXSortColumn::Status: ItemsToSort.Sort(CompareStatus);
		break;
	case EWraithXSortColumn::Type: ItemsToSort.Sort(CompareType);
		break;
	case EWraithXSortColumn::Size: ItemsToSort.Sort(CompareSize);
		break;
	default:
		break;
	}
}

bool FWraithXViewModel::MatchSearchCondition(const TSharedPtr<FCoDAsset>& Item, const TArray<FString>& SearchTokens)
{
	if (!Item.IsValid()) return false;

	if (SearchTokens.IsEmpty()) return true;

	for (const FString& Token : SearchTokens)
	{
		if (!Item->AssetName.Contains(Token, ESearchCase::IgnoreCase))
		{
			return false;
		}
	}
	return true;
}

void FWraithXViewModel::ShowNotification(const FText& Message)
{
	ExecuteDelegateOnGameThread(OnShowNotificationDelegate, Message);
}
