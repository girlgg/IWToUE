#pragma once

#include "CoreMinimal.h"
#include "Interface/IAssetImporter.h"

struct FCastRoot;
struct FWraithXModel;
struct FCastImportOptions;
struct FImportedAssetsCache;
struct FCastMaterialInfo;
struct FWraithXMaterial;
struct FWraithXMap;
struct FWraithMapMeshData;
class ICastMeshImporter;
class ICastMaterialImporter;

class FMapImporter final : public IAssetImporter
{
public:
	FMapImporter();
	virtual bool Import(const FAssetImportContext& Context, TArray<UObject*>& OutCreatedObjects,
	                    FOnAssetImportProgress ProgressDelegate) override;
	virtual UClass* GetSupportedUClass() const override;

private:
	static UStaticMesh* ImportMapMeshChunk(
		const FWraithMapMeshData& MapMeshData,
		const FCastMaterialInfo& MaterialInfo,
		const FAssetImportContext& Context,
		ICastMeshImporter* MeshImporter,
		ICastMaterialImporter* MaterialImporter
	);

	static UObject* ImportStaticModelAsset(
		uint64 ModelPtr,
		const FAssetImportContext& Context,
		ICastMaterialImporter* MaterialImporter,
		ICastMeshImporter* MeshImporter
	);

	static bool ProcessModelDependencies_MapHelper(
		FWraithXModel& InOutModelData,
		FCastRoot& OutSceneRoot,
		const FAssetImportContext& Context,
		const FString& BaseTexturePath);

	static void PlaceActorsInLevel_GameThread(
		const FWraithXMap& MapData,
		const TArray<TWeakObjectPtr<UStaticMesh>>& ImportedMapMeshes,
		FImportedAssetsCache& Cache
	);

	static bool PrepareMaterialInfo(
		uint64 MaterialPtr,
		FWraithXMaterial& InMaterialData,
		FCastMaterialInfo& OutMaterialInfo,
		const FAssetImportContext& Context,
		const FString& BaseTexturePath
	);

	static UTexture2D* ProcessTexture(
		uint64 ImagePtr,
		FString& InOutImageName,
		const FAssetImportContext& Context,
		const FString& BaseTexturePath
	);

	static UMaterialInterface* GetOrCreateMaterialAsset(
		const FCastMaterialInfo& MaterialInfo,
		ICastMaterialImporter* MaterialImporter,
		const FString& PreferredPackagePath,
		const FAssetImportContext& Context
	);

	TSharedPtr<ICastMeshImporter> MeshImporterInternal;
	TSharedPtr<ICastMaterialImporter> MaterialImporterInternal;
};
