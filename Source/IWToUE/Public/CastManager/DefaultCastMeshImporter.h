#pragma once
#include "Interface/ICastAssetImporter.h"

namespace SkeletalMeshImportData
{
	struct FBone;
	struct FMaterial;
}

struct FPreparedStatMeshData
{
	bool bIsValid = false;
	FString MeshName;
	FString ParentPackagePath;
	FString OriginalFilePath;

	TArray<TTuple<int32, float>> LodModelIndexAndDistance;

	FCastRoot* RootPtr = nullptr;
	const FCastImportOptions* OptionsPtr = nullptr;
	ICastMaterialImporter* MaterialImporterPtr = nullptr;
};

struct FPreparedSkelMeshData
{
	bool bIsValid = false;
	FString MeshName;
	FString ParentPackagePath;
	FString OriginalFilePath;

	TArray<SkeletalMeshImportData::FBone> RefBonesBinary;

	TArray<TTuple<int32, float>> LodModelIndexAndDistance;

	FCastRoot* RootPtr = nullptr; // Pointer to the relevant root
	const FCastImportOptions* OptionsPtr = nullptr; // Pointer to options
	ICastMaterialImporter* MaterialImporterPtr = nullptr;

	int32 BaseModelIndex = -1;
};

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
	FPreparedSkelMeshData PrepareSkeletalMeshData_OffThread(
		FCastScene& CastScene,
		const FCastImportOptions& Options,
		UObject* InParent,
		FName Name,
		ICastMaterialImporter* MaterialImporter,
		const FString& OriginalFilePath
	);

	FPreparedStatMeshData PrepareStaticMeshData_OffThread(
		FCastScene& CastScene,
		const FCastImportOptions& Options,
		UObject* InParent,
		FName Name,
		ICastMaterialImporter* MaterialImporter,
		const FString& OriginalFilePath);

	USkeletalMesh* CreateAndApplySkeletalMeshData_GameThread(
		FPreparedSkelMeshData& PreparedData,
		EObjectFlags Flags,
		TArray<UObject*>& OutCreatedObjects);

	UStaticMesh* CreateAndApplyStaticMeshData_GameThread(
		FPreparedStatMeshData& PreparedData,
		EObjectFlags Flags,
		TArray<UObject*>& OutCreatedObjects);

	bool PopulateMeshDescriptionFromCastModel(
		const FCastRoot& Root,
		const FCastModelInfo& ModelInfo,
		FMeshDescription& OutMeshDescription,
		const FCastImportOptions& Options,
		ICastMaterialImporter* MaterialImporter,
		UObject* AssetOuter,
		TMap<uint32, TPair<FName, UMaterialInterface*>>& SharedMaterialMap,
		TArray<FName>& UniqueMaterialSlotNames,
		TArray<UObject*>& OutCreatedObjects);

	bool PopulateSkeletalMeshImportData(
		const FCastRoot& Root,
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
