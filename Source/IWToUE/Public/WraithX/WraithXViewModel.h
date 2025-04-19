#pragma once

#include "CoreMinimal.h"
#include "Templates/Tuple.h"
#include "Async/Async.h"
#include "Templates/UnrealTemplate.h"

struct FCoDAsset;
class FAssetImportManager;

UENUM()
enum class EWraithXSortColumn : uint8
{
	None,
	AssetName,
	Status,
	Type,
	Size
};

FName EWraithXSortColumnToFName(EWraithXSortColumn Column);
EWraithXSortColumn FNameToEWraithXSortColumn(FName Name);

class FWraithXViewModel : public TSharedFromThis<FWraithXViewModel>
{
public:
	FWraithXViewModel();
	~FWraithXViewModel();

	/** Initializes the ViewModel, creating dependencies and binding delegates. */
	void Initialize(TSharedPtr<FAssetImportManager> InAssetImportManager);

	/** Cleans up resources and unbinds delegates. Call before destruction if needed explicitly. */
	void Cleanup();

	// --- Delegates for View Notification ---

	/** Notifies the View that the filtered/sorted list of items has changed. */
	DECLARE_DELEGATE(FOnListChanged);
	FOnListChanged OnListChangedDelegate;

	/** Notifies the View about the current loading progress (0.0 to 1.0). */
	DECLARE_DELEGATE_OneParam(FOnLoadingProgressChanged, float);
	FOnLoadingProgressChanged OnLoadingProgressChangedDelegate;

	/** Notifies the View that the loading state (true/false) has changed. */
	DECLARE_DELEGATE_OneParam(FOnLoadingStateChanged, bool);
	FOnLoadingStateChanged OnLoadingStateChangedDelegate;

	/** Notifies the View that the asset count (Total/Filtered) has changed. */
	DECLARE_DELEGATE_TwoParams(FOnAssetCountChanged, int32 /*Total*/, int32 /*Filtered*/);
	FOnAssetCountChanged OnAssetCountChangedDelegate;

	/** Requests the View to show a notification message. */
	DECLARE_DELEGATE_OneParam(FOnShowNotification, const FText& /*Message*/);
	FOnShowNotification OnShowNotificationDelegate;

	/** Notifies the View that the Import Path has changed (e.g., after browsing). */
	DECLARE_DELEGATE_OneParam(FOnImportPathChanged, const FString& /*NewPath*/);
	FOnImportPathChanged OnImportPathChangedDelegate;

	/** Requests the View to open a preview for the given asset. */
	DECLARE_DELEGATE_OneParam(FOnPreviewAsset, TSharedPtr<FCoDAsset> /*AssetToPreview*/);
	FOnPreviewAsset OnPreviewAssetDelegate;

	// --- Getters for View Data Binding ---

	/** Gets the currently displayed list of filtered and sorted assets. */
	const TArray<TSharedPtr<FCoDAsset>>& GetFilteredItems() const;

	const TArray<TSharedPtr<FCoDAsset>>& GetSelectedItems() const;

	/** Gets the current loading progress (0.0 to 1.0). */
	float GetLoadingProgress() const;

	/** Returns true if an operation (load/refresh/import) is in progress. */
	bool IsLoading() const;

	/** Gets the total number of assets loaded (before filtering). */
	int32 GetTotalAssetCount() const;

	/** Gets the number of assets currently displayed after filtering. */
	int32 GetFilteredAssetCount() const;

	/** Gets the current import path string. */
	const FString& GetImportPath() const;

	/** Gets the current optional parameters string. */
	const FString& GetOptionalParams() const;

	/** Gets the current sort mode for a specific column FName. */
	EColumnSortMode::Type GetSortModeForColumn(FName ColumnId) const;

	/** Gets the current search text. */
	const FString& GetSearchText() const;

	// --- UI调用的命令 ---

	/** Sets the search text and triggers filtering. */
	void SetSearchText(const FString& InText);

	/** Clears the search text and triggers filtering. */
	void ClearSearchText();

	/** Initiates loading the initial game asset data. */
	void LoadGame();

	/** Initiates refreshing the game asset data. */
	void RefreshGame();

	/** Initiates importing the currently selected assets. */
	void ImportSelectedAssets();

	/** Initiates importing all currently filtered assets. */
	void ImportAllAssets();

	/** Sets the import path, performing validation. */
	void SetImportPath(const FString& Path);

	/** Sets the optional import parameters string. */
	void SetOptionalParams(const FString& Params);

	/** Opens a directory browser to select the import path. */
	void BrowseImportPath(TSharedPtr<SWindow> ParentWindow);

	/** Sets the column to sort by and toggles the sort mode. */
	void SetSortColumn(FName ColumnId);

	/** Updates the ViewModel's knowledge of which items are selected in the View. */
	void SetSelectedItems(const TArray<TSharedPtr<FCoDAsset>>& InSelectedItems);

	/** Called when an asset item is double-clicked in the View. */
	void RequestPreviewAsset(TSharedPtr<FCoDAsset> Item);

private:
	/** Sets the loading state and notifies the View. */
	void SetLoadingState(bool bNewLoadingState);

	/** Updates the loading progress and notifies the View. Ensures call is on GameThread. */
	void UpdateLoadingProgress(float InProgress);

	/** Handles the completion callback from FAssetImportManager. */
	void HandleAssetInitCompleted();

	/** Handles the progress callback from FAssetImportManager. */
	void HandleAssetLoadingProgress(float InProgress);

	/** Performs filtering and sorting asynchronously. */
	void ApplyFilteringAndSortingAsync();

	/** Cancels any ongoing filtering/sorting task. */
	void CancelCurrentAsyncTask();

	/** Filters the AllLoadedAssets based on CurrentSearchText. (Runs on background thread) */
	TArray<TSharedPtr<FCoDAsset>> FilterAssets_WorkerThread(const TArray<TSharedPtr<FCoDAsset>>& AssetsToFilter,
	                                                        const FString& SearchText);

	/** Sorts the FilteredItems based on CurrentSortColumn and CurrentSortMode. (Runs on GameThread or background) */
	void SortDataInternal(TArray<TSharedPtr<FCoDAsset>>& ItemsToSort);

	/** Updates the ViewModel's internal state after async task completes. (Runs on GameThread) */
	void FinalizeUpdate_GameThread(TArray<TSharedPtr<FCoDAsset>> NewFilteredItems, int32 NewTotalCount);

	/** Helper to check if an item matches the current search criteria. */
	bool MatchSearchCondition(const TSharedPtr<FCoDAsset>& Item, const TArray<FString>& SearchTokens);

	/** Helper function to safely execute delegates on the Game Thread */
	template <typename DelegateType, typename... ParamTypes>
	void ExecuteDelegateOnGameThread(DelegateType& Delegate, ParamTypes... Params);

	/** Shows a notification via the delegate */
	void ShowNotification(const FText& Message);

	TSharedPtr<FAssetImportManager> AssetImportManager;
	TArray<TSharedPtr<FCoDAsset>> AllLoadedAssets;
	TArray<TSharedPtr<FCoDAsset>> FilteredItems;
	TArray<TSharedPtr<FCoDAsset>> SelectedItems;

	FCriticalSection DataLock;

	FString CurrentSearchText;
	FString ImportPath;
	FString OptionalParams;

	int32 TotalAssetCount = 0;
	float CurrentLoadingProgress = 0.0f;

	EWraithXSortColumn CurrentSortColumn = EWraithXSortColumn::None;
	EColumnSortMode::Type CurrentSortMode = EColumnSortMode::None;

	/** Handle for the active async filtering/sorting task */
	TFuture<TArray<TSharedPtr<FCoDAsset>>> CurrentAsyncTask;

	bool bIsLoading = false;

	friend class SWraithXWidget;
};

template <typename T, typename = void>
struct THasBroadcast : std::false_type {};

template <typename T>
struct THasBroadcast<T, std::void_t<decltype(std::declval<T>().Broadcast())>> : std::true_type {};

template <typename DelegateType, typename... ParamTypes>
void FWraithXViewModel::ExecuteDelegateOnGameThread(DelegateType& Delegate, ParamTypes... Params)
{
	auto ParamsTuple = std::make_tuple(std::forward<ParamTypes>(Params)...);

	auto Invoke = [&Delegate, ParamsTuple = std::move(ParamsTuple)]() mutable
	{
		std::apply([&Delegate]<typename... T0>(T0&&... args) 
		{
			if constexpr (THasBroadcast<DelegateType>::value)
			{
				Delegate.Broadcast(std::forward<T0>(args)...);
			}
			else
			{
				Delegate.ExecuteIfBound(std::forward<T0>(args)...);
			}
		}, ParamsTuple);
	};
    
	if (IsInGameThread())
	{
		Invoke();
	}
	else
	{
		AsyncTask(ENamedThreads::GameThread, MoveTemp(Invoke));
	}
}
