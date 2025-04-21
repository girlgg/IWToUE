#pragma once
#include "Interface/ICastAssetImporter.h"

namespace SkeletalMeshImportData
{
	struct FMaterial;
}

class FDefaultCastMeshImporter : public ICastMeshImporter
{
public:
	virtual UStaticMesh* ImportStaticMesh(FCastScene& CastScene, const FCastImportOptions& Options,
	                                      UObject* InParent, FName Name, EObjectFlags Flags,
	                                      ICastMaterialImporter* MaterialImporter,
	                                      FString OriginalFilePath, TArray<UObject*>& OutCreatedObjects) override;
	virtual USkeletalMesh* ImportSkeletalMesh(FCastScene& CastScene, const FCastImportOptions& Options,
	                                          UObject* InParent, FName Name, EObjectFlags Flags,
	                                          ICastMaterialImporter* MaterialImporter, FString OriginalFilePath,
	                                          TArray<UObject*>& OutCreatedObjects) override;

protected:
	bool PopulateMeshDescriptionFromCastModel(
		const FCastModelInfo& ModelInfo,
		FMeshDescription& OutMeshDescription,
		const FCastImportOptions& Options,
		ICastMaterialImporter* MaterialImporter,
		UObject* AssetOuter,
		TMap<uint32, TPair<FName, UMaterialInterface*>>& SharedMaterialMap,
		TArray<FName>& UniqueMaterialSlotNames,
		TArray<UObject*>& OutCreatedObjects);

	bool PopulateSkeletalMeshImportData(
		const FCastModelInfo& ModelInfo,
		FSkeletalMeshImportData& OutImportData,
		const FCastImportOptions& Options,
		ICastMaterialImporter* MaterialImporter,
		USkeletalMesh* SkeletalMesh,
		TMap<uint32, int32>& SharedMaterialMap,
		TArray<SkeletalMeshImportData::FMaterial>& FinalMaterials,
		TArray<UObject*>& OutCreatedObjects,
		bool& bOutHasVertexColors,
		int32& MaxUVLayer);

private:
	void EnsureSkeletonAndPhysicsAsset(USkeletalMesh* SkeletalMesh, const FCastImportOptions& Options,
	                                   UObject* ParentPackage, TArray<UObject*>& OutCreatedObjects);
};
