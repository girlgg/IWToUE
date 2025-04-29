#pragma once

struct FImportedAssetsCache;
class FAssetImportManager;
class IGameAssetHandler;
struct FCoDAsset;

/** @brief Holds context information for a single asset import operation. */
struct FAssetImportContext
{
	/** Base Content Browser path for this import session (e.g., /Game/Imports/MyMap). */
	FString BaseImportPath;
	/** Optional parameters string passed to the import manager. */
	FString OptionalParameters;
	/** Shared pointer to the source asset information discovered by GameProcess. */
	TSharedPtr<FCoDAsset> SourceAsset;
	/** Pointer to the game-specific handler for reading raw data. */
	IGameAssetHandler* GameHandler = nullptr;
	/** Pointer to the manager coordinating the import, used for callbacks and cache access. */
	FAssetImportManager* ImportManager = nullptr;
	/** Shared cache for assets created during this import session. */
	FImportedAssetsCache* AssetCache = nullptr;

	/** Helper to safely cast SourceAsset to its specific type. */
	template <typename T>
	TSharedPtr<T> GetAssetInfo() const
	{
		return StaticCastSharedPtr<T>(SourceAsset);
	}
};

/** @brief Delegate for reporting progress of a single asset's import (0.0 to 1.0). */
DECLARE_DELEGATE_TwoParams(FOnAssetImportProgress, float /*Fraction*/, const FText& /*Status*/);

/** @brief Interface for importing a specific type of asset into Unreal Engine. */
class IAssetImporter
{
public:
	virtual ~IAssetImporter() = default;

	/**
	 * @brief Imports a single asset based on the provided context.
	 * @param Context Contains all necessary information for the import.
	 * @param OutCreatedObjects Array populated by the importer with any newly created UObjects (primary and dependencies).
	 * @param ProgressDelegate Delegate to report progress specific to this asset's import.
	 * @return True if the import was successful (or partially successful), false on critical failure.
	 */
	virtual bool Import(const FAssetImportContext& Context, TArray<UObject*>& OutCreatedObjects,
	                    FOnAssetImportProgress ProgressDelegate) = 0;

	/**
	 * @brief Gets the primary Unreal Engine class this importer typically creates.
	 * Useful for logging or filtering. Can return nullptr if multiple classes are possible or not applicable.
	 */
	virtual UClass* GetSupportedUClass() const = 0;
};

/** @brief Shared cache for assets created during an import session. */
struct FImportedAssetsCache
{
	/** Maps game asset pointer/hash to the imported UTexture2D. */
	TMap<uint64, TWeakObjectPtr<UTexture2D>> ImportedTextures;
	/** Maps game asset pointer/hash to the imported UMaterialInterface. */
	TMap<uint64, TWeakObjectPtr<UMaterialInterface>> ImportedMaterials;
	/** Maps game asset pointer/hash to the imported primary model asset (Static or Skeletal). */
	TMap<uint64, TWeakObjectPtr<UObject>> ImportedModels;

	/** Set of all packages that were created or modified and need saving. */
	TSet<UPackage*> PackagesToSave;
	/** Direct list of all assets created (useful for tracking). */
	TArray<UObject*> AllCreatedAssets;

	/** Critical section to protect access to the maps. */
	FCriticalSection CacheMutex;

	/** Adds an asset to the tracking lists and marks its package for saving. Thread-safe. */
	void AddCreatedAsset(UObject* Asset)
	{
		if (!Asset) return;

		FScopeLock Lock(&CacheMutex);
		AllCreatedAssets.AddUnique(Asset);
		UPackage* Pkg = Asset->GetOutermost();
		if (Pkg && Pkg != GetTransientPackage())
		{
			PackagesToSave.Add(Pkg);
			Pkg->MarkPackageDirty();
		}
	}

	/** Adds multiple assets. Thread-safe. */
	void AddCreatedAssets(const TArray<UObject*>& Assets)
	{
		FScopeLock Lock(&CacheMutex);
		for (UObject* Asset : Assets)
		{
			if (!Asset) continue;
			AllCreatedAssets.AddUnique(Asset);
			UPackage* Pkg = Asset->GetOutermost();
			if (Pkg && Pkg != GetTransientPackage())
			{
				PackagesToSave.Add(Pkg);
				Pkg->MarkPackageDirty();
			}
		}
	}

	/** Clears all caches and tracking lists. Not thread-safe, call during initialization or shutdown. */
	void Clear()
	{
		ImportedTextures.Empty();
		ImportedMaterials.Empty();
		ImportedModels.Empty();
		PackagesToSave.Empty();
		AllCreatedAssets.Empty();
	}
};
