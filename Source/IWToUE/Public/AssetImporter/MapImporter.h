#pragma once

#include "CoreMinimal.h"
#include "Interface/IAssetImporter.h"

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
	virtual bool Import(const FString& ImportPath, TSharedPtr<FCoDAsset> Asset, IGameAssetHandler* Handler,
	                    FAssetImportManager* Manager) override;

private:
	UStaticMesh* ImportMapMeshChunk(
		const FWraithMapMeshData& MapMeshData, const FCastMaterialInfo& MaterialInfo, const FString& BaseImportPath,
		ICastMeshImporter* MeshImporter, ICastMaterialImporter* MaterialImporter, FImportedAssetsCache& Cache);
	UObject* ImportStaticModelAsset(uint64 ModelPtr, const FString& BaseImportPath, IGameAssetHandler* Handler,
	                                ICastMaterialImporter* MaterialImporter, ICastMeshImporter* MeshImporter,
	                                FImportedAssetsCache& Cache);

	void PlaceActorsInLevel(
		const FWraithXMap& MapData, const TArray<TWeakObjectPtr<UStaticMesh>>& ImportedMapMeshes,
		FImportedAssetsCache& Cache);

	bool PrepareMaterialInfo(uint64 MaterialPtr, FWraithXMaterial& InMaterialData, FCastMaterialInfo& OutMaterialInfo,
	                         IGameAssetHandler* Handler, const FString& BaseTexturePath, FImportedAssetsCache& Cache);

	UTexture2D* ProcessTexture(uint64 ImagePtr, FString& InOutImageName, IGameAssetHandler* Handler,
	                           const FString& BaseTexturePath, FImportedAssetsCache& Cache);

	void SaveImportedAssets(FImportedAssetsCache& Cache);

	UMaterialInterface* GetOrCreateMaterialAsset(
		const FCastMaterialInfo& MaterialInfo, ICastMaterialImporter* MaterialImporter,
		const FString& PreferredPackagePath, const FCastImportOptions& Options, FImportedAssetsCache& Cache);

	FCriticalSection AssetMapMutex;
};
