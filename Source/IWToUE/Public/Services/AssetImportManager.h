#pragma once

#include "Interface/IAssetImporter.h"
#include "UObject/Object.h"

class FGameProcess;
class IGameAssetHandler;
enum class EWraithAssetType : uint8;
class IAssetImporter;
struct FCoDAsset;

// Delegate for overall import process completion
DECLARE_DELEGATE_OneParam(FOnImportProcessCompleted, bool /*bSuccess*/);
// Delegate for overall import process progress
DECLARE_DELEGATE_OneParam(FOnImportProcessProgress, float /*OverallFraction (0.0 - 1.0)*/);
// Delegate for overall asset discovery process completion
DECLARE_MULTICAST_DELEGATE(FOnDiscoveryProcessCompleted);
// Delegate for overall asset discovery process progress
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDiscoveryProcessProgress, float /*OverallFraction (0.0 - 1.0)*/);

/**
 * @class FAssetImportManager
 * @brief Manages the overall asset import process from discovered assets to Unreal assets.
 * Handles initialization, discovery triggering, asynchronous import execution,
 * progress reporting, and dependency management (Handlers, Importers, Cache).
 */
class FAssetImportManager : public TSharedFromThis<FAssetImportManager>
{
public:
	FAssetImportManager();
	virtual ~FAssetImportManager();

	// --- Initialization and Lifecycle ---

	/**
	 * Initializes the manager. Creates GameProcess, determines Game Handler, sets up Importers.
	 * Must be called before any other operations.
	 * @return True if initialization was successful, false otherwise.
	 */
	bool Initialize();

	/** Cleans up resources, unbinds delegates, destroys GameProcess. */
	void Shutdown();

	// --- Asset Discovery Control ---

	/** Starts the asynchronous process of discovering assets in the target game via GameProcess. */
	void StartAssetDiscovery();

	/** Returns true if asset discovery or asset import is currently in progress. */
	bool IsBusy() const;

	// --- Asset Access ---

	/** Gets the list of assets currently discovered by the GameProcess. */
	TArray<TSharedPtr<FCoDAsset>> GetDiscoveredAssets() const;

	// --- Import Operations ---

	/**
	* Imports a selection of discovered assets into the Unreal project asynchronously.
	* Use delegates (OnImportProgressDelegate, OnImportCompletedDelegate) for feedback.
	* @param BaseImportPath The root content browser path for imported assets (e.g., "/Game/Imports/MyMap").
	* @param AssetsToImport The list of discovered assets (from GetDiscoveredAssets) to import.
	* @param OptionalParams Additional string parameters potentially used by importers.
	* @return True if the import process was successfully started, false otherwise (e.g., already busy, not initialized).
	*/
	bool ImportAssetsAsync(const FString& BaseImportPath, TArray<TSharedPtr<FCoDAsset>> AssetsToImport,
	                       const FString& OptionalParams);

	// --- Delegates ---

	/** Broadcast when the initial asset discovery process completes. */
	FOnDiscoveryProcessCompleted OnDiscoveryCompletedDelegate;
	/** Broadcast periodically during the asset discovery phase (0.0 to 1.0). Provides overall progress. */
	FOnDiscoveryProcessProgress OnDiscoveryProgressDelegate;

	/** Broadcast periodically during the asset import phase (0.0 to 1.0). Provides overall progress. */
	FOnImportProcessProgress OnImportProgressDelegate;
	/** Broadcast when the entire asynchronous import process finishes. Parameter indicates overall success. */
	FOnImportProcessCompleted OnImportCompletedDelegate;

	// --- Internal Access (Used by Importers/Context) ---

	/** Gets the currently active game asset handler. Returns nullptr if not initialized correctly. */
	IGameAssetHandler* GetGameHandler() const { return CurrentGameHandler.Get(); }

	/** Provides access to the shared cache for imported assets (textures, materials, models). */
	FImportedAssetsCache& GetImportCache() { return ImportCache; }

	/** Critical section for protecting access to the shared cache. */
	FCriticalSection& GetCacheLock() { return CacheLock; }

private:
	// --- Callbacks from GameProcess ---

	void HandleDiscoveryCompleteInternal();
	void HandleDiscoveryProgressInternal(float Progress);

	// --- Asynchronous Import Task ---

	/** The actual import work performed on a background thread. */
	void RunImportTask(FString BaseImportPath, TArray<TSharedPtr<FCoDAsset>> AssetsToImport, FString OptionalParams);

	// --- Internal Helpers ---

	/** Registers the default set of asset importers (Model, Texture, etc.). */
	void SetupAssetImporters();
	/** Creates/updates the game-specific asset handler based on GameProcess info. */
	bool UpdateGameHandler();
	/** Registers a specific importer instance for an asset type. */
	void RegisterImporter(EWraithAssetType Type, const TSharedPtr<IAssetImporter>& Importer);
	/** Retrieves the appropriate importer for a given asset type. */
	IAssetImporter* GetImporterForAsset(EWraithAssetType Type);
	/** Saves all packages collected in the ImportCache to disk. Must be called on Game Thread. */
	void SaveCachedAssets_GameThread();

	// --- State ---

	TSharedPtr<FGameProcess> ProcessInstance;
	TSharedPtr<IGameAssetHandler> CurrentGameHandler;
	TMap<EWraithAssetType, TSharedPtr<IAssetImporter>> AssetImporters;
	FImportedAssetsCache ImportCache; // Shared cache for textures, materials etc.

	/** Protects access to the AssetImporters map. */
	FCriticalSection ImporterMapLock;
	/** Protects access to the ImportCache maps. */
	FCriticalSection CacheLock;

	/** Flag indicating if asset discovery background task is running. */
	std::atomic<bool> bIsDiscoveringAssets{false};
	/** Flag indicating if asset import background task is running. */
	std::atomic<bool> bIsImportingAssets{false};
};
