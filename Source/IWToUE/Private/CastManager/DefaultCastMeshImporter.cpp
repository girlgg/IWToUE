#include "CastManager/DefaultCastMeshImporter.h"

#include "IMeshBuilderModule.h"
#include "MaterialDomain.h"
#include "PhysicsAssetUtils.h"
#include "StaticMeshAttributes.h"
#include "CastManager/CastImportOptions.h"
#include "CastManager/CastModel.h"
#include "CastManager/CastRoot.h"
#include "EditorFramework/AssetImportData.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Rendering/SkeletalMeshLODImporterData.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "StaticMeshDescription.h"

UStaticMesh* FDefaultCastMeshImporter::ImportStaticMesh(FCastScene& CastScene,
                                                        const FCastImportOptions& Options,
                                                        UObject* InParent,
                                                        FName Name,
                                                        EObjectFlags Flags,
                                                        ICastMaterialImporter* MaterialImporter,
                                                        FString OriginalFilePath,
                                                        TArray<UObject*>& OutCreatedObjects)
{
	FPreparedStatMeshData PreparedData = PrepareStaticMeshData_OffThread(
		CastScene, Options, InParent, Name, MaterialImporter, OriginalFilePath);
	if (!PreparedData.bIsValid)
	{
		UE_LOG(LogCast, Error, TEXT("Failed to prepare static mesh data for %s."), *Name.ToString());
		return nullptr;
	}

	UStaticMesh* ResultStaticMesh;

	if (IsInGameThread())
	{
		ResultStaticMesh = CreateAndApplyStaticMeshData_GameThread(PreparedData, Flags, OutCreatedObjects);
	}
	else
	{
		TPromise<UStaticMesh*> Promise;
		TFuture<UStaticMesh*> Future = Promise.GetFuture();

		AsyncTask(ENamedThreads::GameThread,
		          [PreparedData = MoveTemp(PreparedData),
			          Flags,
			          &OutCreatedObjects,
			          this,
			          Promise = MoveTemp(Promise)
		          ]() mutable
		          {
			          UStaticMesh* Result = CreateAndApplyStaticMeshData_GameThread(
				          PreparedData, Flags, OutCreatedObjects);
			          Promise.SetValue(Result);
		          });

		ResultStaticMesh = Future.Get();
	}

	return ResultStaticMesh;
}

USkeletalMesh* FDefaultCastMeshImporter::ImportSkeletalMesh(FCastScene& CastScene,
                                                            const FCastImportOptions& Options,
                                                            UObject* InParent,
                                                            FName Name,
                                                            EObjectFlags Flags,
                                                            ICastMaterialImporter* MaterialImporter,
                                                            FString OriginalFilePath,
                                                            TArray<UObject*>& OutCreatedObjects)
{
	FPreparedSkelMeshData PreparedData = PrepareSkeletalMeshData_OffThread(
		CastScene, Options, InParent, Name, MaterialImporter, OriginalFilePath);

	if (!PreparedData.bIsValid)
	{
		UE_LOG(LogCast, Error, TEXT("Failed to prepare skeletal mesh data for %s."), *Name.ToString());
		return nullptr;
	}

	USkeletalMesh* ResultSkeletalMesh = nullptr;

	if (IsInGameThread())
	{
		ResultSkeletalMesh = CreateAndApplySkeletalMeshData_GameThread(
			PreparedData, Flags, OutCreatedObjects);
	}
	else
	{
		TPromise<USkeletalMesh*> Promise;
		TFuture<USkeletalMesh*> Future = Promise.GetFuture();

		AsyncTask(ENamedThreads::GameThread,
		          [PreparedData = MoveTemp(PreparedData),
			          Flags,
			          &OutCreatedObjects,
			          this,
			          Promise = MoveTemp(Promise)
		          ]() mutable
		          {
			          USkeletalMesh* Result = CreateAndApplySkeletalMeshData_GameThread(
				          PreparedData, Flags, OutCreatedObjects);
			          Promise.SetValue(Result);
		          });

		ResultSkeletalMesh = Future.Get();
	}

	return ResultSkeletalMesh;
}

FPreparedSkelMeshData FDefaultCastMeshImporter::PrepareSkeletalMeshData_OffThread(
	FCastScene& CastScene, const FCastImportOptions& Options, UObject* InParent, FName Name,
	ICastMaterialImporter* MaterialImporter, const FString& OriginalFilePath)
{
	FPreparedSkelMeshData PreparedData;
	PreparedData.MeshName = ObjectTools::SanitizeObjectName(Name.ToString());
	PreparedData.ParentPackagePath = InParent->IsA<UPackage>()
		                                 ? InParent->GetPathName()
		                                 : FPaths::GetPath(InParent->GetPathName());
	PreparedData.OriginalFilePath = OriginalFilePath;
	PreparedData.OptionsPtr = &Options; // Store pointer
	PreparedData.MaterialImporterPtr = MaterialImporter; // Store pointer

	// --- Initial Checks ---
	if (CastScene.Roots.IsEmpty())
	{
		UE_LOG(LogCast, Error, TEXT("Prepare: No CastRoot data provided."));
		return PreparedData;
	}
	if (!MaterialImporter)
	{
		UE_LOG(LogCast, Error, TEXT("Prepare: Material Importer is null."));
		return PreparedData;
	}

	// **Store pointer to Root - Ensure CastScene lifetime!**
	PreparedData.RootPtr = &CastScene.Roots[0];
	FCastRoot& Root = *PreparedData.RootPtr; // Use reference locally

	// Handle default LOD info if missing (Modifies Root directly)
	if (Root.ModelLodInfo.IsEmpty())
	{
		if (Root.Models.IsEmpty())
		{
			UE_LOG(LogCast, Error, TEXT("Prepare: No ModelLodInfo and no Models found for '%s'."), *Name.ToString());
			return PreparedData;
		}
		Root.ModelLodInfo.AddDefaulted();
		Root.ModelLodInfo[0].ModelIndex = 0;
		Root.ModelLodInfo[0].Distance = 0.0f;
		UE_LOG(LogCast, Warning,
		       TEXT("Prepare: No ModelLodInfo found for '%s'. Creating default LOD0 entry pointing to Models[0]."),
		       *Name.ToString());
	}

	// --- Find Base Model (Off-Thread Safe) ---
	const FCastModelInfo* BaseModelInfoPtr = nullptr;
	if (Root.ModelLodInfo.Num() > 0 && Root.Models.IsValidIndex(Root.ModelLodInfo[0].ModelIndex))
	{
		PreparedData.BaseModelIndex = Root.ModelLodInfo[0].ModelIndex;
		BaseModelInfoPtr = &Root.Models[PreparedData.BaseModelIndex];
	}
	else if (Root.Models.Num() > 0)
	{
		PreparedData.BaseModelIndex = 0;
		BaseModelInfoPtr = &Root.Models[0];
		UE_LOG(LogCast, Warning,
		       TEXT(
			       "Prepare: Could not find valid base model from ModelLodInfo[0]. Using Models[0] for skeleton definition."
		       ));
	}
	if (!BaseModelInfoPtr)
	{
		UE_LOG(LogCast, Error, TEXT("Prepare: Cannot find a valid base FCastModelInfo for %s."), *Name.ToString());
		return PreparedData;
	}
	const FCastModelInfo& BaseModelInfo = *BaseModelInfoPtr;

	// --- Process Skeleton (Off-Thread Safe) ---
	int32 TotalBoneOffset = 0;
	PreparedData.RefBonesBinary.Reserve(BaseModelInfo.Skeletons.Num() * 30); // Pre-allocate estimate
	for (const FCastSkeletonInfo& Skeleton : BaseModelInfo.Skeletons)
	{
		for (const FCastBoneInfo& Bone : Skeleton.Bones)
		{
			// (Populate BoneBinary as before...)
			SkeletalMeshImportData::FBone BoneBinary;
			BoneBinary.Name = Bone.BoneName;
			BoneBinary.ParentIndex = Bone.ParentIndex == -1 ? INDEX_NONE : Bone.ParentIndex + TotalBoneOffset;
			BoneBinary.NumChildren = 0;
			SkeletalMeshImportData::FJointPos& JointMatrix = BoneBinary.BonePos;
			JointMatrix.Transform.SetTranslation(FVector3f(Bone.LocalPosition.X, -Bone.LocalPosition.Y,
			                                               Bone.LocalPosition.Z));
			FQuat4f RawBoneRotator(Bone.LocalRotation.X, -Bone.LocalRotation.Y, Bone.LocalRotation.Z,
			                       -Bone.LocalRotation.W);
			JointMatrix.Transform.SetRotation(RawBoneRotator);
			JointMatrix.Transform.SetScale3D(FVector3f(Bone.Scale));
			PreparedData.RefBonesBinary.Add(BoneBinary);
		}
		TotalBoneOffset += Skeleton.Bones.Num();
	}
	// Calculate NumChildren (as before)
	for (int32 BoneIdx = 0; BoneIdx < PreparedData.RefBonesBinary.Num(); ++BoneIdx)
	{
		const int32 ParentIdx = PreparedData.RefBonesBinary[BoneIdx].ParentIndex;
		if (ParentIdx != INDEX_NONE && PreparedData.RefBonesBinary.IsValidIndex(ParentIdx))
		{
			PreparedData.RefBonesBinary[ParentIdx].NumChildren++;
		}
	}

	// --- Collect LOD Info (Off-Thread Safe) ---
	PreparedData.LodModelIndexAndDistance.Reserve(Root.ModelLodInfo.Num());
	for (const FCastModelLod& LodInfo : Root.ModelLodInfo)
	{
		// Store index and distance needed for GT loop
		PreparedData.LodModelIndexAndDistance.Emplace(LodInfo.ModelIndex, LodInfo.Distance);
	}

	if (PreparedData.LodModelIndexAndDistance.IsEmpty())
	{
		UE_LOG(LogCast, Error, TEXT("Prepare: No LOD information collected for %s."), *PreparedData.MeshName);
		return PreparedData;
	}

	PreparedData.bIsValid = true;
	return PreparedData;
}

FPreparedStatMeshData FDefaultCastMeshImporter::PrepareStaticMeshData_OffThread(FCastScene& CastScene,
	const FCastImportOptions& Options, UObject* InParent, FName Name, ICastMaterialImporter* MaterialImporter,
	const FString& OriginalFilePath)
{
	FPreparedStatMeshData PreparedData;

	if (!MaterialImporter)
	{
		UE_LOG(LogCast, Error, TEXT("PrepareStat: Cannot import static mesh: Material Importer is null."));
		return PreparedData;
	}
	if (CastScene.Roots.IsEmpty())
	{
		UE_LOG(LogCast, Warning, TEXT("PrepareStat: Cannot import static mesh '%s': No mesh data provided."),
		       *Name.ToString());
		return PreparedData;
	}

	FCastRoot& Root = CastScene.Roots[0];

	if (Root.ModelLodInfo.IsEmpty())
	{
		if (Root.Models.IsEmpty())
		{
			UE_LOG(LogCast, Error, TEXT("PrepareStat: No ModelLodInfo and no Models found for '%s'."),
			       *Name.ToString());
			return PreparedData;
		}
		Root.ModelLodInfo.AddDefaulted();
		Root.ModelLodInfo[0].ModelIndex = 0;
		Root.ModelLodInfo[0].Distance = 0.0f;
		UE_LOG(LogCast, Warning,
		       TEXT("PrepareStat: No ModelLodInfo found for '%s'. Creating default LOD0 entry pointing to Models[0]."),
		       *Name.ToString());
	}

	PreparedData.MeshName = NoIllegalSigns(Name.ToString());
	PreparedData.ParentPackagePath = InParent->IsA<UPackage>()
		                                 ? InParent->GetPathName()
		                                 : FPaths::GetPath(InParent->GetPathName());
	PreparedData.OriginalFilePath = OriginalFilePath;

	PreparedData.OptionsPtr = &Options;
	PreparedData.MaterialImporterPtr = MaterialImporter;

	PreparedData.LodModelIndexAndDistance.Reserve(Root.ModelLodInfo.Num());
	for (const FCastModelLod& LodInfo : Root.ModelLodInfo)
	{
		PreparedData.LodModelIndexAndDistance.Emplace(LodInfo.ModelIndex, LodInfo.Distance);
	}

	if (PreparedData.LodModelIndexAndDistance.IsEmpty())
	{
		UE_LOG(LogCast, Error, TEXT("PrepareStat: No valid LOD information collected for %s."), *PreparedData.MeshName);
		return PreparedData;
	}

	PreparedData.bIsValid = true;
	return PreparedData;
}

USkeletalMesh* FDefaultCastMeshImporter::CreateAndApplySkeletalMeshData_GameThread(FPreparedSkelMeshData& PreparedData,
	EObjectFlags Flags, TArray<UObject*>& OutCreatedObjects)
{
	if (!PreparedData.bIsValid || !PreparedData.RootPtr || !PreparedData.OptionsPtr || !PreparedData.
		MaterialImporterPtr)
	{
		UE_LOG(LogCast, Error, TEXT("GT_Apply: Prepared data is invalid or pointers are null for %s."),
		       *PreparedData.MeshName);
		return nullptr;
	}

	FCastRoot& Root = *PreparedData.RootPtr; // Dereference pointer safely now
	const FCastImportOptions& Options = *PreparedData.OptionsPtr;
	ICastMaterialImporter* MaterialImporter = PreparedData.MaterialImporterPtr;

	// Use a local slow task for GT operations
	FScopedSlowTask SlowTask(100, NSLOCTEXT("CastImporter", "ImportingSkeletalMeshGT",
	                                        "Importing Skeletal Mesh (GT)..."));
	SlowTask.MakeDialog();

	// --- Create Asset (Game Thread) ---
	SlowTask.EnterProgressFrame(5, FText::Format(
		                            NSLOCTEXT("CastImporter", "SkM_CreatingAssetGT", "Creating Asset {0} (GT)"),
		                            FText::FromString(PreparedData.MeshName)));
	// Assuming CreateAsset is thread-safe or called correctly (as refactored previously)
	USkeletalMesh* SkeletalMesh = ICastAssetImporter::CreateAsset<USkeletalMesh>(
		PreparedData.ParentPackagePath, PreparedData.MeshName, true);
	if (!SkeletalMesh)
	{
		UE_LOG(LogCast, Error, TEXT("GT_Apply: Failed to create USkeletalMesh asset object: %s"),
		       *PreparedData.MeshName);
		return nullptr;
	}
	OutCreatedObjects.Add(SkeletalMesh); // Add main asset

	// --- Shared Data Across LODs (Now managed on GT) ---
	TMap<uint32, int32> SharedMaterialMap;
	TArray<SkeletalMeshImportData::FMaterial> FinalMaterials;
	bool bHasVertexColors_AnyLOD = false;
	int MaxUVLayer_AnyLOD = 0;
	TArray<FVector> AllMeshPoints_LOD0;

	// --- Setup Skeletal Mesh Asset Base (Game Thread) ---
	SkeletalMesh->PreEditChange(nullptr);
	SkeletalMesh->InvalidateDeriveDataCacheGUID();
	SkeletalMesh->ResetLODInfo();

	FSkeletalMeshModel* ImportedModel = SkeletalMesh->GetImportedModel();
	check(ImportedModel);
	ImportedModel->LODModels.Empty();

	// --- Process Each LOD (Game Thread - includes Populate call now) ---
	float ProgressPerLOD = 40.0f / FMath::Max(1, PreparedData.LodModelIndexAndDistance.Num());
	int32 ValidLodsProcessed = 0;

	for (int32 LodIdx = 0; LodIdx < PreparedData.LodModelIndexAndDistance.Num(); ++LodIdx)
	{
		const int32 CurrentModelIndex = PreparedData.LodModelIndexAndDistance[LodIdx].Get<0>();
		const float CurrentDistance = PreparedData.LodModelIndexAndDistance[LodIdx].Get<1>();

		SlowTask.EnterProgressFrame(ProgressPerLOD,
		                            FText::Format(
			                            NSLOCTEXT("CastImporter", "SkM_ProcessingLODGT", "Processing LOD {0} (GT)..."),
			                            FText::AsNumber(ValidLodsProcessed))); // Use ValidLodsProcessed for UI index

		// --- Validate Model Index and Data (Game Thread) ---
		if (!Root.Models.IsValidIndex(CurrentModelIndex))
		{
			UE_LOG(LogCast, Error, TEXT("GT_Apply: Invalid ModelIndex %d specified for LOD %d in '%s'. Skipping."),
			       CurrentModelIndex, ValidLodsProcessed, *PreparedData.MeshName);
			continue;
		}
		const FCastModelInfo& CurrentLodModelInfo = Root.Models[CurrentModelIndex];
		if (CurrentLodModelInfo.Meshes.IsEmpty())
		{
			UE_LOG(LogCast, Warning,
			       TEXT("GT_Apply: ModelInfo at index %d (LOD %d) has no mesh data in '%s'. Skipping."),
			       CurrentModelIndex, ValidLodsProcessed, *PreparedData.MeshName);
			continue;
		}

		// --- Populate Import Data (Game Thread) ---
		FSkeletalMeshImportData LodImportData;
		LodImportData.RefBonesBinary = PreparedData.RefBonesBinary; // Use prepared skeleton

		// **Call Populate ON GAME THREAD, passing the valid SkeletalMesh**
		bool bPopulateSuccess = PopulateSkeletalMeshImportData(
			Root,
			CurrentLodModelInfo,
			LodImportData,
			Options,
			MaterialImporter,
			SkeletalMesh, // Pass the created mesh
			SharedMaterialMap, // Use local GT map
			FinalMaterials, // Use local GT list
			OutCreatedObjects, // Allow adding materials etc. directly
			bHasVertexColors_AnyLOD, // Use local GT flag
			MaxUVLayer_AnyLOD // Use local GT max
		);

		if (!bPopulateSuccess || LodImportData.Points.IsEmpty() || LodImportData.Faces.IsEmpty())
		{
			UE_LOG(LogCast, Error,
			       TEXT(
				       "GT_Apply: PopulateSkeletalMeshImportData failed or result empty for LOD %d (Model %d) in %s. Skipping."
			       ), ValidLodsProcessed, CurrentModelIndex, *PreparedData.MeshName);
			continue; // Skip this LOD
		}

		// Store points from LOD0 for bounding box calculation (only if this is the first valid LOD)
		if (ValidLodsProcessed == 0)
		{
			AllMeshPoints_LOD0.Reserve(LodImportData.Points.Num());
			for (const FVector3f& Pt : LodImportData.Points) AllMeshPoints_LOD0.Add(FVector(Pt));
		}

		// --- Add LOD to Asset (Game Thread) ---
		FSkeletalMeshLODInfo& AssetLODInfo = SkeletalMesh->AddLODInfo();
		// AssetLODInfo.ScreenSize = CurrentDistance / 10;

		FSkeletalMeshBuildSettings BuildOptions;
		BuildOptions.bRecomputeNormals = !LodImportData.bHasNormals;
		BuildOptions.bRecomputeTangents = !LodImportData.bHasTangents;
		BuildOptions.bUseMikkTSpace = BuildOptions.bRecomputeTangents;
		// ... (set other build options as before) ...
		AssetLODInfo.BuildSettings = BuildOptions;

		ImportedModel->LODModels.Add(new FSkeletalMeshLODModel());
		FSkeletalMeshLODModel& LODModel = ImportedModel->LODModels.Last();
		// Use Last() as index matches ValidLodsProcessed
		LODModel.NumTexCoords = FMath::Max(1, MaxUVLayer_AnyLOD);

		// Save the populated import data
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		SkeletalMesh->SaveLODImportedData(ValidLodsProcessed, LodImportData); // Use ValidLodsProcessed as the index
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

		ValidLodsProcessed++; // Increment count of successfully processed LODs
	}

	// Check if any valid LODs were actually processed and added
	if (ValidLodsProcessed == 0) // Or check SkeletalMesh->GetLODNum()
	{
		UE_LOG(LogCast, Error, TEXT("GT_Apply: No valid LODs were imported/processed for Skeletal Mesh %s. Aborting."),
		       *PreparedData.MeshName);
		SkeletalMesh->MarkAsGarbage(); // Clean up the asset
		return nullptr;
	}


	SlowTask.EnterProgressFrame(5, NSLOCTEXT("CastImporter", "SkM_SetupRefSkelGT",
	                                         "Setting Up Reference Skeleton (GT)..."));

	TArray<FSkeletalMaterial>& MeshMaterials = SkeletalMesh->GetMaterials();
	MeshMaterials.Empty(FinalMaterials.Num());
	for (const auto& MatData : FinalMaterials)
	{
		UMaterialInterface* MaterialAsset = MatData.Material.Get();
		if (!MaterialAsset) MaterialAsset = UMaterial::GetDefaultMaterial(MD_Surface);
		FName SlotName = FName(*MatData.MaterialImportName);
		MeshMaterials.Add(FSkeletalMaterial(MaterialAsset, true, false, SlotName, SlotName));
	}

	{
		FReferenceSkeletonModifier RefSkelModifier(SkeletalMesh->GetRefSkeleton(), SkeletalMesh->GetSkeleton());
		for (const auto& BoneData : PreparedData.RefBonesBinary)
		{
			FName BoneFName = FName(BoneData.Name);
			if (BoneFName.IsNone()) continue;
			FMeshBoneInfo BoneInfo(BoneFName, BoneData.Name, BoneData.ParentIndex);
			FTransform BoneTransform(BoneData.BonePos.Transform);
			RefSkelModifier.Add(BoneInfo, BoneTransform);
		}
	}

	SlowTask.EnterProgressFrame(10, NSLOCTEXT("CastImporter", "SkM_SetupAssetGT", "Setting Up Asset (GT)..."));
	SkeletalMesh->SetHasVertexColors(bHasVertexColors_AnyLOD); // Use flag updated during Populate
	SkeletalMesh->SetVertexColorGuid(bHasVertexColors_AnyLOD ? FGuid::NewGuid() : FGuid());
	FBox BoundingBox(AllMeshPoints_LOD0.GetData(), AllMeshPoints_LOD0.Num());
	SkeletalMesh->SetImportedBounds(FBoxSphereBounds(BoundingBox));

	// Build Mesh
	SlowTask.EnterProgressFrame(15, NSLOCTEXT("CastImporter", "SkM_BuildingMeshLODsGT", "Building Mesh LODs (GT)..."));
	IMeshBuilderModule& MeshBuilderModule = FModuleManager::LoadModuleChecked<IMeshBuilderModule>("MeshBuilder");
	SkeletalMesh->Build(); // This still happens on GT

	// Verify Render Data
	SlowTask.EnterProgressFrame(2, NSLOCTEXT("CastImporter", "SkM_VerifyRenderDataGT",
	                                         "Verifying Render Data (GT)..."));
	FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetResourceForRendering();
	if (!RenderData || RenderData->LODRenderData.Num() != ValidLodsProcessed || !RenderData->LODRenderData.
		IsValidIndex(0) || RenderData->LODRenderData[0].GetNumVertices() == 0)
	{
		UE_LOG(LogCast, Error,
		       TEXT("GT_Apply: Failed to generate valid RenderData for %s after building. Num LODs mismatch or empty."),
		       *PreparedData.MeshName);
		SkeletalMesh->MarkAsGarbage();
		return nullptr;
	}

	// Asset Import Data
	if (!PreparedData.OriginalFilePath.IsEmpty())
	{
		// (Setup AssetImportData as before...)
		UAssetImportData* ImportData = SkeletalMesh->GetAssetImportData();
		if (!ImportData)
		{
			ImportData = NewObject<UAssetImportData>(SkeletalMesh, TEXT("AssetImportData"));
			SkeletalMesh->SetAssetImportData(ImportData);
		}
		ImportData->Update(FPaths::ConvertRelativePathToFull(PreparedData.OriginalFilePath));
	}

	// Finalize
	SlowTask.EnterProgressFrame(3, NSLOCTEXT("CastImporter", "SkM_FinalizingGT", "Finalizing (GT)..."));
	SkeletalMesh->CalculateInvRefMatrices();

	// Ensure Skeleton and Physics Asset (happens on GT)
	EnsureSkeletonAndPhysicsAsset(SkeletalMesh, Options, /*InParent?*/ SkeletalMesh->GetPackage(), OutCreatedObjects);
	// Pass Package or correct parent

	SkeletalMesh->PostEditChange();
	SkeletalMesh->MarkPackageDirty();

	UE_LOG(LogCast, Log, TEXT("GT_Apply: Successfully imported Skeletal Mesh with %d LODs: %s"),
	       SkeletalMesh->GetLODNum(), *SkeletalMesh->GetPathName());

	return SkeletalMesh;
}

UStaticMesh* FDefaultCastMeshImporter::CreateAndApplyStaticMeshData_GameThread(FPreparedStatMeshData& PreparedData,
                                                                               EObjectFlags Flags,
                                                                               TArray<UObject*>& OutCreatedObjects)
{
	check(IsInGameThread());

	if (!PreparedData.bIsValid || !PreparedData.RootPtr || !PreparedData.OptionsPtr || !PreparedData.
		MaterialImporterPtr)
	{
		UE_LOG(LogCast, Error, TEXT("GT_ApplyStat: Prepared data is invalid or pointers are null for %s."),
		       *PreparedData.MeshName);
		return nullptr;
	}

	FCastRoot& Root = *PreparedData.RootPtr;
	const FCastImportOptions& Options = *PreparedData.OptionsPtr;
	ICastMaterialImporter* MaterialImporter = PreparedData.MaterialImporterPtr;

	FScopedSlowTask SlowTask(100, NSLOCTEXT("CastImporter", "ImportingStaticMeshGT", "Importing Static Mesh (GT)..."));
	SlowTask.MakeDialog();

	// --- Create Static Mesh Asset (Game Thread) ---
	SlowTask.EnterProgressFrame(5, FText::Format(
		                            NSLOCTEXT("CastImporter", "StM_CreatingAssetGT", "Creating Asset {0} (GT)"),
		                            FText::FromString(PreparedData.MeshName)));
	UStaticMesh* StaticMesh = CreateAsset<UStaticMesh>(PreparedData.ParentPackagePath, PreparedData.MeshName, true);
	if (!StaticMesh)
	{
		UE_LOG(LogCast, Error, TEXT("GT_ApplyStat: Failed to create UStaticMesh asset object: %s in package %s"),
		       *PreparedData.MeshName, *PreparedData.ParentPackagePath);
		return nullptr;
	}
	OutCreatedObjects.Add(StaticMesh);
	StaticMesh->PreEditChange(nullptr);

	// --- Shared Data Across LODs (Managed on Game Thread) ---
	TMap<uint32, TPair<FName, UMaterialInterface*>> SharedMaterialMap;
	TArray<FName> UniqueMaterialSlotNames;
	bool bHasNormals_AnyLOD = false;
	bool bHasTangents_AnyLOD = false;
	int MaxUVChannels_AnyLOD = 1;

	StaticMesh->SetNumSourceModels(0);

	// --- Process Each LOD (Game Thread) ---
	float ProgressPerLOD = 70.0f / FMath::Max(1, PreparedData.LodModelIndexAndDistance.Num());
	int32 ValidLodsProcessed = 0;

	for (int32 LodBuildIndex = 0; LodBuildIndex < PreparedData.LodModelIndexAndDistance.Num(); ++LodBuildIndex)
	{
		const int32 CurrentModelIndex = PreparedData.LodModelIndexAndDistance[LodBuildIndex].Get<0>();
		const float CurrentDistance = PreparedData.LodModelIndexAndDistance[LodBuildIndex].Get<1>();

		SlowTask.EnterProgressFrame(ProgressPerLOD,
		                            FText::Format(
			                            NSLOCTEXT("CastImporter", "StM_ProcessingLODGT", "Processing LOD {0} (GT)..."),
			                            FText::AsNumber(ValidLodsProcessed)));

		// --- Validate Model Index and Data (Game Thread) ---
		if (!Root.Models.IsValidIndex(CurrentModelIndex))
		{
			UE_LOG(LogCast, Error, TEXT("GT_ApplyStat: Invalid ModelIndex %d specified for LOD %d in '%s'. Skipping."),
			       CurrentModelIndex, ValidLodsProcessed, *PreparedData.MeshName);
			continue;
		}
		const FCastModelInfo& ModelInfo = Root.Models[CurrentModelIndex];
		if (ModelInfo.Meshes.IsEmpty())
		{
			UE_LOG(LogCast, Warning,
			       TEXT("GT_ApplyStat: ModelInfo at index %d (LOD %d) has no mesh data in '%s'. Skipping."),
			       CurrentModelIndex, ValidLodsProcessed, *PreparedData.MeshName);
			continue;
		}

		UE_LOG(LogCast, Log, TEXT("GT_ApplyStat: Processing LOD %d using ModelIndex %d for %s"), ValidLodsProcessed,
		       CurrentModelIndex, *PreparedData.MeshName);

		FMeshDescription MeshDescription;
		bool bPopulateSuccess = PopulateMeshDescriptionFromCastModel(
			Root,
			ModelInfo,
			MeshDescription,
			Options,
			MaterialImporter,
			StaticMesh,
			SharedMaterialMap,
			UniqueMaterialSlotNames,
			OutCreatedObjects);

		if (!bPopulateSuccess || MeshDescription.Vertices().Num() == 0 || MeshDescription.Polygons().Num() == 0)
		{
			UE_LOG(LogCast, Error,
			       TEXT(
				       "GT_ApplyStat: Failed to populate mesh description or result was empty for LOD %d in %s. Skipping this LOD."
			       ),
			       ValidLodsProcessed, *PreparedData.MeshName);
			continue;
		}

		// --- Update Mesh Stats (Game Thread) ---
		FStaticMeshAttributes Attributes(MeshDescription);
		if (Attributes.GetVertexInstanceNormals().IsValid() && Attributes.GetVertexInstanceNormals().GetNumElements() >
			0)
			bHasNormals_AnyLOD = true;
		if (Attributes.GetVertexInstanceTangents().IsValid() && Attributes.GetVertexInstanceTangents().GetNumElements()
			> 0)
			bHasTangents_AnyLOD = true;
		MaxUVChannels_AnyLOD = FMath::Max(MaxUVChannels_AnyLOD, Attributes.GetVertexInstanceUVs().GetNumChannels());

		// --- Add Source Model and Configure Build Settings (Game Thread) ---
		FStaticMeshSourceModel& SrcModel = StaticMesh->AddSourceModel();

		SrcModel.BuildSettings.bRecomputeNormals = !bHasNormals_AnyLOD;
		SrcModel.BuildSettings.bRecomputeTangents = !bHasTangents_AnyLOD || !bHasNormals_AnyLOD;
		SrcModel.BuildSettings.bUseMikkTSpace = SrcModel.BuildSettings.bRecomputeTangents;
		SrcModel.BuildSettings.bGenerateLightmapUVs = (ValidLodsProcessed == 0);
		SrcModel.BuildSettings.SrcLightmapIndex = 0;
		SrcModel.BuildSettings.DstLightmapIndex = MaxUVChannels_AnyLOD;
		SrcModel.BuildSettings.bRemoveDegenerates = true;
		SrcModel.BuildSettings.bUseHighPrecisionTangentBasis = false;
		SrcModel.BuildSettings.bUseFullPrecisionUVs = false;

		// Screen size setting
		// float ScreenSize = FMath::Clamp(10 / FMath::Max(1.0f, CurrentDistance), 0.01f, 1.0f);
		// SrcModel.ScreenSize.Default = ScreenSize;

		// --- Commit Mesh Description (Game Thread) ---
		FMeshDescription* StaticMeshDescription = StaticMesh->CreateMeshDescription(ValidLodsProcessed);
		if (StaticMeshDescription)
		{
			*StaticMeshDescription = MoveTemp(MeshDescription);
			StaticMesh->CommitMeshDescription(ValidLodsProcessed);
			UE_LOG(LogCast, Log, TEXT("GT_ApplyStat: Committed MeshDescription for LOD %d to StaticMesh %s"),
			       ValidLodsProcessed, *PreparedData.MeshName);
		}
		else
		{
			UE_LOG(LogCast, Error, TEXT("GT_ApplyStat: Failed to get or create mesh description for LOD %d on %s"),
			       ValidLodsProcessed, *PreparedData.MeshName);
		}

		ValidLodsProcessed++;
	}

	// --- Post-LOD Processing (Game Thread) ---
	if (StaticMesh->GetNumSourceModels() == 0)
	{
		UE_LOG(LogCast, Error, TEXT("GT_ApplyStat: No valid LODs were imported for %s. Aborting."),
		       *PreparedData.MeshName);
		StaticMesh->ClearFlags(RF_Public | RF_Standalone);
		StaticMesh->MarkAsGarbage();
		OutCreatedObjects.Remove(StaticMesh);
		return nullptr;
	}

	SlowTask.EnterProgressFrame(5, NSLOCTEXT("CastImporter", "StM_SettingMaterialsGT", "Setting Materials (GT)..."));

	// --- Set Static Materials (Game Thread) ---
	TArray<FStaticMaterial> StaticMaterials;
	StaticMaterials.Reserve(UniqueMaterialSlotNames.Num());
	for (FName SlotName : UniqueMaterialSlotNames)
	{
		bool bFound = false;
		for (auto const& [Hash, MatInfoPair] : SharedMaterialMap)
		{
			if (MatInfoPair.Key == SlotName)
			{
				StaticMaterials.Add(FStaticMaterial(MatInfoPair.Value, SlotName, SlotName));
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			UE_LOG(LogCast, Error,
			       TEXT(
				       "GT_ApplyStat: Could not find material for slot name %s during final material setup for %s. Using default."
			       ),
			       *SlotName.ToString(), *PreparedData.MeshName);
			StaticMaterials.Add(FStaticMaterial(UMaterial::GetDefaultMaterial(MD_Surface), SlotName, SlotName));
		}
	}
	StaticMesh->SetStaticMaterials(StaticMaterials);

	// --- Set Section Info (Game Thread) ---
	SlowTask.EnterProgressFrame(5, NSLOCTEXT("CastImporter", "StM_SettingSectionsGT", "Setting Sections (GT)..."));
	StaticMesh->GetSectionInfoMap().Clear();
	StaticMesh->GetOriginalSectionInfoMap().Clear();
	for (int32 LodIdx = 0; LodIdx < StaticMesh->GetNumSourceModels(); ++LodIdx)
	{
		const FMeshDescription* CommittedMeshDesc = StaticMesh->GetMeshDescription(LodIdx);
		if (!CommittedMeshDesc)
		{
			UE_LOG(LogCast, Warning,
			       TEXT("GT_ApplyStat: Could not retrieve committed MeshDescription for LOD %d when setting sections."),
			       LodIdx);
			continue;
		}

		TPolygonGroupAttributesConstRef<FName> PolygonGroupMaterialSlotNames =
			CommittedMeshDesc->PolygonGroupAttributes().GetAttributesRef<FName>(
				MeshAttribute::PolygonGroup::ImportedMaterialSlotName);

		if (!PolygonGroupMaterialSlotNames.IsValid())
		{
			UE_LOG(LogCast, Warning, TEXT("GT_ApplyStat: Missing PolygonGroupMaterialSlotNames attribute for LOD %d."),
			       LodIdx);
			continue;
		}

		int32 SectionIndex = 0;
		for (FPolygonGroupID PolygonGroupID : CommittedMeshDesc->PolygonGroups().GetElementIDs())
		{
			FName SlotName = PolygonGroupMaterialSlotNames[PolygonGroupID];
			int32 MaterialIndex = StaticMaterials.IndexOfByPredicate([&](const FStaticMaterial& M)
			{
				return M.MaterialSlotName == SlotName;
			});
			if (MaterialIndex == INDEX_NONE)
			{
				UE_LOG(LogCast, Error,
				       TEXT(
					       "GT_ApplyStat: Failed to find MaterialIndex for SlotName '%s' in LOD %d, Section %d. Using 0."
				       ),
				       *SlotName.ToString(), LodIdx, SectionIndex);
				MaterialIndex = 0;
			}

			FMeshSectionInfo Info;
			Info.MaterialIndex = MaterialIndex;
			Info.bCastShadow = true;
			Info.bEnableCollision = true;
			StaticMesh->GetSectionInfoMap().Set(LodIdx, SectionIndex, Info);
			SectionIndex++;
		}
	}
	StaticMesh->GetOriginalSectionInfoMap().CopyFrom(StaticMesh->GetSectionInfoMap());

	// --- Build and Finalize (Game Thread) ---
	SlowTask.EnterProgressFrame(10, FText::Format(
		                            NSLOCTEXT("CastImporter", "StM_BuildingMeshGT", "Building Mesh {0} (GT)..."),
		                            FText::FromString(PreparedData.MeshName)));
	StaticMesh->Build(false);

	SlowTask.EnterProgressFrame(2, NSLOCTEXT("CastImporter", "StM_InitResourcesGT", "Initializing Resources (GT)..."));
	StaticMesh->InitResources();
	StaticMesh->SetLightingGuid();


	// --- Asset Import Data (Game Thread) ---
	SlowTask.EnterProgressFrame(3, NSLOCTEXT("CastImporter", "StM_SettingImportDataGT", "Setting Import Data (GT)..."));
	if (!PreparedData.OriginalFilePath.IsEmpty())
	{
		UAssetImportData* ImportData = StaticMesh->GetAssetImportData();
		if (!ImportData)
		{
			ImportData = NewObject<UAssetImportData>(StaticMesh, TEXT("AssetImportData"));
			StaticMesh->SetAssetImportData(ImportData);
		}
		if (ImportData)
		{
			FString FullPath = FPaths::ConvertRelativePathToFull(PreparedData.OriginalFilePath);
			ImportData->Update(FullPath);
		}
	}

	// --- Final Touches (Game Thread) ---
	StaticMesh->PostEditChange();
	StaticMesh->MarkPackageDirty();

	return StaticMesh;
}

bool FDefaultCastMeshImporter::PopulateMeshDescriptionFromCastModel(
	const FCastRoot& Root,
	const FCastModelInfo& ModelInfo,
	FMeshDescription& OutMeshDescription,
	const FCastImportOptions& Options,
	ICastMaterialImporter* MaterialImporter,
	UObject* AssetOuter,
	TMap<uint32, TPair<FName, UMaterialInterface*>>& SharedMaterialMap,
	TArray<FName>& UniqueMaterialSlotNames,
	TArray<UObject*>& OutCreatedObjects)
{
	FStaticMeshAttributes Attributes(OutMeshDescription);
	Attributes.Register();
	Attributes.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector2f> VertexInstanceUVs = Attributes.GetVertexInstanceUVs();
	TVertexInstanceAttributesRef<FVector3f> VertexInstanceNormals = Attributes.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector3f> VertexInstanceTangents = Attributes.GetVertexInstanceTangents();
	TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
	TVertexInstanceAttributesRef<FVector4f> VertexInstanceColors = Attributes.GetVertexInstanceColors();
	TPolygonGroupAttributesRef<FName> PolygonGroupImportedMaterialSlotNames =
		Attributes.GetPolygonGroupMaterialSlotNames();

	// Determine required UV channels and presence of colors/normals for THIS model
	int32 MaxUVLayer = 0;
	bool bHasVertexColors = false;
	bool bHasNormals = false;
	bool bHasTangents = false;
	int32 TotalVertexCount = 0;
	int32 TotalTriangleCount = 0;

	for (const FCastMeshInfo& Mesh : ModelInfo.Meshes)
	{
		TotalVertexCount += Mesh.VertexPositions.Num();
		TotalTriangleCount += Mesh.Faces.Num() / 3;
		MaxUVLayer = FMath::Max(MaxUVLayer, (int32)Mesh.UVLayer);
		if (!Mesh.VertexColor.IsEmpty()) bHasVertexColors = true;
		if (!Mesh.VertexNormals.IsEmpty()) bHasNormals = true;
		if (!Mesh.VertexTangents.IsEmpty()) bHasTangents = true;
	}

	// Set number of UV channels (always at least 1)
	int NumUVChannels = FMath::Max(1, MaxUVLayer + 1);
	VertexInstanceUVs.SetNumChannels(NumUVChannels); // Set UV channels for THIS mesh description

	// Reserve space
	OutMeshDescription.ReserveNewVertices(TotalVertexCount);
	OutMeshDescription.ReserveNewVertexInstances(TotalTriangleCount * 3);
	OutMeshDescription.ReserveNewPolygons(TotalTriangleCount);
	OutMeshDescription.ReserveNewEdges(TotalTriangleCount * 3);

	TMap<uint32, FPolygonGroupID> CurrentLodMaterialHashToPolygonGroup;

	// --- Populate Mesh Description from FCastMeshInfo ---
	UE_LOG(LogCast, Verbose, TEXT("Populating MeshDescription... Total Vertices: %d, Total Triangles: %d"),
	       TotalVertexCount, TotalTriangleCount);

	int32 CurrentVertexOffset = 0; // Relative offset within this ModelInfo
	for (const FCastMeshInfo& Mesh : ModelInfo.Meshes)
	{
		if (Mesh.VertexPositions.IsEmpty() || Mesh.Faces.IsEmpty())
		{
			UE_LOG(LogCast, Warning, TEXT("Skipping mesh '%s' due to missing vertex or face data."), *Mesh.Name);
			continue;
		}

		// --- Ensure Material and Polygon Group Exist ---
		FPolygonGroupID PolygonGroupID;
		FName MaterialSlotName;
		UMaterialInterface* UnrealMaterial = nullptr;

		if (TPair<FName, UMaterialInterface*>* FoundMaterialInfo = SharedMaterialMap.Find(Mesh.MaterialHash))
		{
			MaterialSlotName = FoundMaterialInfo->Key;
			UnrealMaterial = FoundMaterialInfo->Value;
			if (FPolygonGroupID* FoundGroupID = CurrentLodMaterialHashToPolygonGroup.Find(Mesh.MaterialHash))
			{
				PolygonGroupID = *FoundGroupID;
			}
			else
			{
				PolygonGroupID = OutMeshDescription.CreatePolygonGroup();
				CurrentLodMaterialHashToPolygonGroup.Add(Mesh.MaterialHash, PolygonGroupID);
				PolygonGroupImportedMaterialSlotNames[PolygonGroupID] = MaterialSlotName;
			}
		}
		else
		{
			PolygonGroupID = OutMeshDescription.CreatePolygonGroup();
			CurrentLodMaterialHashToPolygonGroup.Add(Mesh.MaterialHash, PolygonGroupID);

			if (Root.Materials.IsValidIndex(Mesh.MaterialIndex) &&
				Root.Materials[Mesh.MaterialIndex].MaterialHash == Mesh.MaterialHash)
			{
				const FCastMaterialInfo& CastMaterial = Root.Materials[Mesh.MaterialIndex];

				MaterialSlotName = FName(CastMaterial.Name);
				FName OriginalSlotName = MaterialSlotName;
				int32 Suffix = 1;
				while (UniqueMaterialSlotNames.Contains(MaterialSlotName))
				{
					MaterialSlotName = FName(FString::Printf(TEXT("%s_%d"), *OriginalSlotName.ToString(), Suffix++));
				}
				UniqueMaterialSlotNames.Add(MaterialSlotName);

				UnrealMaterial = MaterialImporter->CreateMaterialInstance(CastMaterial, Options, AssetOuter);
				if (!UnrealMaterial)
				{
					UE_LOG(LogCast, Warning,
					       TEXT(
						       "Failed to create material instance '%s' for mesh '%s' (Hash: %llu). Using default material."
					       ),
					       *CastMaterial.Name, *Mesh.Name, Mesh.MaterialHash);
					UnrealMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
				}
				else if (UnrealMaterial != UMaterial::GetDefaultMaterial(MD_Surface))
				{
					OutCreatedObjects.Add(UnrealMaterial);
				}
			}
			else
			{
				UE_LOG(LogCast, Error,
				       TEXT(
					       "Invalid MaterialIndex %d or hash mismatch for MaterialHash %llu on mesh %s. Check pre-processing step. Using default material."
				       ),
				       Mesh.MaterialIndex, Mesh.MaterialHash, *Mesh.Name);
				UnrealMaterial = UMaterial::GetDefaultMaterial(MD_Surface);

				MaterialSlotName = FName(FString::Printf(TEXT("DefaultMaterial_%llu"), Mesh.MaterialHash));
				FName OriginalSlotName = MaterialSlotName;
				int32 Suffix = 1;
				while (UniqueMaterialSlotNames.Contains(MaterialSlotName))
				{
					MaterialSlotName = FName(FString::Printf(TEXT("%s_%d"), *OriginalSlotName.ToString(), Suffix++));
				}
				UniqueMaterialSlotNames.Add(MaterialSlotName);
			}

			SharedMaterialMap.Add(Mesh.MaterialHash,
			                      TPair<FName, UMaterialInterface*>(MaterialSlotName, UnrealMaterial));

			PolygonGroupImportedMaterialSlotNames[PolygonGroupID] = MaterialSlotName;
		}

		// ---  Create Vertices ---
		TArray<FVertexID> CurrentMeshVertexIDs;
		CurrentMeshVertexIDs.SetNumUninitialized(Mesh.VertexPositions.Num());
		for (int32 i = 0; i < Mesh.VertexPositions.Num(); ++i)
		{
			const FVertexID VertexID = OutMeshDescription.CreateVertex();
			// Apply coordinate system conversion
			Attributes.GetVertexPositions()[VertexID] = FVector3f(Mesh.VertexPositions[i].X,
			                                                      -Mesh.VertexPositions[i].Y,
			                                                      Mesh.VertexPositions[i].Z);
			CurrentMeshVertexIDs[i] = VertexID;
		}

		// --- Create Faces (Polygons and Vertex Instances) ---
		TArray VertexOrder = {0, 1, 2};
		if (Options.bReverseFace)
		{
			VertexOrder = {2, 1, 0};
		}

		for (int32 FaceIdx = 0; FaceIdx < Mesh.Faces.Num(); FaceIdx += 3)
		{
			TArray<FVertexInstanceID> TriangleVertexInstanceIDs;
			TriangleVertexInstanceIDs.SetNum(3);
			bool bTriangleIsValid = true;

			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				int32 OrderedCornerIndex = VertexOrder[Corner];
				int32 MeshLocalVertexIndex = Mesh.Faces[FaceIdx + OrderedCornerIndex];

				if (!CurrentMeshVertexIDs.IsValidIndex(MeshLocalVertexIndex))
				{
					UE_LOG(LogCast, Error,
					       TEXT("Invalid vertex index %d found in face %d for mesh '%s'. Skipping triangle."),
					       MeshLocalVertexIndex, FaceIdx / 3, *Mesh.Name);
					bTriangleIsValid = false;
					break;
				}
				FVertexID VertexID = CurrentMeshVertexIDs[MeshLocalVertexIndex];
				const FVertexInstanceID VertexInstanceID = OutMeshDescription.CreateVertexInstance(VertexID);
				TriangleVertexInstanceIDs[Corner] = VertexInstanceID;

				// Set UVs
				if (Mesh.VertexUV.IsValidIndex(MeshLocalVertexIndex))
				{
					VertexInstanceUVs.Set(VertexInstanceID, 0,
					                      FVector2f(Mesh.VertexUV[MeshLocalVertexIndex].X,
					                                Mesh.VertexUV[MeshLocalVertexIndex].Y));
				}
				else
				{
					VertexInstanceUVs.Set(VertexInstanceID, 0, FVector2f(0.f, 0.f));
				}
				for (int UVChannel = 1; UVChannel < NumUVChannels; ++UVChannel)
				{
					VertexInstanceUVs.Set(VertexInstanceID, UVChannel, FVector2f(0.f, 0.f));
				}

				// Set Normals, Tangents, Colors
				if (bHasNormals && Mesh.VertexNormals.IsValidIndex(MeshLocalVertexIndex))
				{
					VertexInstanceNormals[VertexInstanceID] = FVector3f(
						Mesh.VertexNormals[MeshLocalVertexIndex].X,
						-Mesh.VertexNormals[MeshLocalVertexIndex].Y,
						Mesh.VertexNormals[MeshLocalVertexIndex].Z).GetSafeNormal();
				}
				if (bHasTangents && Mesh.VertexTangents.IsValidIndex(MeshLocalVertexIndex))
				{
					VertexInstanceTangents[VertexInstanceID] = FVector3f(
						Mesh.VertexTangents[MeshLocalVertexIndex].X,
						-Mesh.VertexTangents[MeshLocalVertexIndex].Y,
						Mesh.VertexTangents[MeshLocalVertexIndex].Z).GetSafeNormal();
					VertexInstanceBinormalSigns[VertexInstanceID] = 1.0f;
				}
				if (bHasVertexColors && Mesh.VertexColor.IsValidIndex(MeshLocalVertexIndex))
				{
					FColor PackedColor(
						(Mesh.VertexColor[MeshLocalVertexIndex] >> 16) & 0xFF,
						(Mesh.VertexColor[MeshLocalVertexIndex] >> 8) & 0xFF,
						(Mesh.VertexColor[MeshLocalVertexIndex] >> 0) & 0xFF,
						(Mesh.VertexColor[MeshLocalVertexIndex] >> 24) & 0xFF
					);
					VertexInstanceColors[VertexInstanceID] = PackedColor.ReinterpretAsLinear();
				}
				else
				{
					VertexInstanceColors[VertexInstanceID] = FLinearColor::White;
				}
			}

			if (bTriangleIsValid)
			{
				OutMeshDescription.CreatePolygon(PolygonGroupID, TriangleVertexInstanceIDs);
			}
		}
		CurrentVertexOffset += Mesh.VertexPositions.Num();
	}

	if (OutMeshDescription.Vertices().Num() == 0 || OutMeshDescription.Polygons().Num() == 0)
	{
		UE_LOG(LogCast, Warning, TEXT("MeshDescription is empty after processing ModelInfo."));
		return false;
	}
	return true;
}

bool FDefaultCastMeshImporter::PopulateSkeletalMeshImportData(const FCastRoot& Root,
                                                              const FCastModelInfo& ModelInfo,
                                                              FSkeletalMeshImportData& OutImportData,
                                                              const FCastImportOptions& Options,
                                                              ICastMaterialImporter* MaterialImporter,
                                                              USkeletalMesh* SkeletalMesh,
                                                              TMap<uint32, int32>& SharedMaterialMap,
                                                              TArray<SkeletalMeshImportData::FMaterial>& FinalMaterials,
                                                              TArray<UObject*>& OutCreatedObjects,
                                                              bool& bOutHasVertexColors, int32& MaxUVLayer)
{
	OutImportData.Points.Empty();
	OutImportData.Wedges.Empty();
	OutImportData.Faces.Empty();
	OutImportData.Influences.Empty();
	OutImportData.Materials.Empty();

	TMap<uint32, int32> LodMaterialMap;

	bool bLodHasNormals = false;
	bool bLodHasTangents = false;
	bool bLodHasColors = false;
	int32 LodMaxUVLayer = 0;
	int32 TotalVertexOffset = 0;

	for (const FCastMeshInfo& Mesh : ModelInfo.Meshes)
	{
		if (Mesh.VertexPositions.IsEmpty() || Mesh.Faces.IsEmpty())
		{
			UE_LOG(LogCast, Warning, TEXT("Skipping skeletal mesh part '%s' due to missing vertex or face data."),
			       *Mesh.Name);
			continue;
		}

		// Vertices (Points) for this LOD
		for (const FVector3f& Pos : Mesh.VertexPositions)
		{
			OutImportData.Points.Add(FVector3f(Pos.X, -Pos.Y, Pos.Z));
		}
		LodMaxUVLayer = FMath::Max(LodMaxUVLayer, (int32)Mesh.UVLayer);
		// --- Material Mapping ---
		int32 LocalMaterialIndex = INDEX_NONE;
		int32 FinalMaterialIndex = INDEX_NONE;
		FString MaterialImportName;
		if (int32* FoundFinalIndexPtr = LodMaterialMap.Find(Mesh.MaterialHash))
		{
			FinalMaterialIndex = *FoundFinalIndexPtr;
			if (int32* FoundLocalIndexPtr = LodMaterialMap.Find(Mesh.MaterialHash))
			{
				LocalMaterialIndex = *FoundLocalIndexPtr;
			}
			if (FinalMaterials.IsValidIndex(FinalMaterialIndex))
			{
				MaterialImportName = FinalMaterials[FinalMaterialIndex].MaterialImportName;
			}
			else
			{
				UE_LOG(LogCast, Error,
				       TEXT(
					       "Populate: SharedMaterialMap points to invalid FinalMaterials index %d for hash %llu. Re-resolving."
				       ), FinalMaterialIndex, Mesh.MaterialHash);
				FinalMaterialIndex = INDEX_NONE;
			}
		}

		if (FinalMaterialIndex == INDEX_NONE)
		{
			UMaterialInterface* UnrealMaterial = nullptr;
			MaterialImportName = FString::Printf(TEXT("MI_Hash_%llu"), Mesh.MaterialHash);
			if (int32* FoundFinalIndex = SharedMaterialMap.Find(Mesh.MaterialHash))
			{
				FinalMaterialIndex = *FoundFinalIndex;
				if (FinalMaterials.IsValidIndex(FinalMaterialIndex))
				{
					UnrealMaterial = FinalMaterials[FinalMaterialIndex].Material.Get();
					MaterialImportName = FinalMaterials[FinalMaterialIndex].MaterialImportName;
				}
				else
				{
					UE_LOG(LogCast, Error,
					       TEXT(
						       "SharedMaterialMap points to invalid FinalMaterials index %d for hash %llu. Re-resolving."
					       ), FinalMaterialIndex, Mesh.MaterialHash);
					FinalMaterialIndex = INDEX_NONE;
				}
			}

			if (FinalMaterialIndex == INDEX_NONE)
			{
				if (Root.Materials.IsValidIndex(Mesh.MaterialIndex) &&
					Root.Materials[Mesh.MaterialIndex].MaterialHash == Mesh.MaterialHash)
				{
					const FCastMaterialInfo& CastMaterial = Root.Materials[Mesh.MaterialIndex];
					FString UserFriendlyName = CastMaterial.Name.IsEmpty() ? MaterialImportName : CastMaterial.Name;

					UnrealMaterial = MaterialImporter->CreateMaterialInstance(CastMaterial, Options, SkeletalMesh);

					if (!UnrealMaterial)
					{
						UE_LOG(LogCast, Warning,
						       TEXT(
							       "Populate: Failed to create material instance %s (Hash: %llu) for mesh %s. Using default."
						       ), *UserFriendlyName, Mesh.MaterialHash, *Mesh.Name);
						UnrealMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
					}
					else if (UnrealMaterial != UMaterial::GetDefaultMaterial(MD_Surface))
					{
						OutCreatedObjects.AddUnique(UnrealMaterial);
					}
				}
				else
				{
					UE_LOG(LogCast, Error,
					       TEXT(
						       "Populate: Invalid MaterialIndex %d or hash mismatch for MaterialHash %llu on mesh %s. Using default material."
					       ), Mesh.MaterialIndex, Mesh.MaterialHash, *Mesh.Name);
					UnrealMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
				}

				SkeletalMeshImportData::FMaterial NewFinalMaterialData;
				NewFinalMaterialData.Material = UnrealMaterial;
				NewFinalMaterialData.MaterialImportName = MaterialImportName;
				FinalMaterialIndex = FinalMaterials.Add(NewFinalMaterialData);

				SharedMaterialMap.Add(Mesh.MaterialHash, FinalMaterialIndex);
			}
			if (LocalMaterialIndex == INDEX_NONE)
			{
				if (int32* FoundLocalIndexPtr = LodMaterialMap.Find(Mesh.MaterialHash))
				{
					LocalMaterialIndex = *FoundLocalIndexPtr;
				}
				else
				{
					// Material doesn't exist locally for this LOD yet, add it.
					// Ensure we have the material pointer (it must exist in FinalMaterials at FinalMaterialIndex)
					UMaterialInterface* MaterialToAdd = nullptr;
					if (FinalMaterials.IsValidIndex(FinalMaterialIndex))
					{
						MaterialToAdd = FinalMaterials[FinalMaterialIndex].Material.Get();
					}
					if (!MaterialToAdd)
					{
						// Should not happen if logic above is correct
						UE_LOG(LogCast, Error,
						       TEXT(
							       "Populate: Could not find material in FinalMaterials for index %d (Hash: %llu). Using default."
						       ), FinalMaterialIndex, Mesh.MaterialHash);
						MaterialToAdd = UMaterial::GetDefaultMaterial(MD_Surface);
					}

					SkeletalMeshImportData::FMaterial NewLocalMaterialData;
					NewLocalMaterialData.Material = MaterialToAdd;
					NewLocalMaterialData.MaterialImportName = MaterialImportName; // Use the HASH name
					LocalMaterialIndex = OutImportData.Materials.Add(NewLocalMaterialData);

					// Add mapping from hash to the index in the local list
					LodMaterialMap.Add(Mesh.MaterialHash, LocalMaterialIndex);
				}
			}
		}

		// Faces and Wedges
		int32 MeshVertexCount = Mesh.VertexPositions.Num();
		TArray VertexOrder = {0, 1, 2};
		if (Options.bReverseFace)
		{
			VertexOrder = {2, 1, 0};
		}

		for (int32 FaceIdx = 0; FaceIdx < Mesh.Faces.Num(); FaceIdx += 3)
		{
			SkeletalMeshImportData::FTriangle& Triangle = OutImportData.Faces.AddZeroed_GetRef();
			Triangle.SmoothingGroups = 1;
			Triangle.MatIndex = LocalMaterialIndex;
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				int32 WedgeGlobalIndex = OutImportData.Wedges.AddUninitialized();
				SkeletalMeshImportData::FVertex& Wedge = OutImportData.Wedges[WedgeGlobalIndex];

				int32 OrderedCornerIndex = VertexOrder[Corner];
				int32 MeshLocalVertexIndex = Mesh.Faces[FaceIdx + OrderedCornerIndex];

				if (MeshLocalVertexIndex < 0 || MeshLocalVertexIndex >= MeshVertexCount)
				{
					UE_LOG(LogCast, Error,
					       TEXT("Invalid vertex index %d found in face %d for mesh '%s'. Skipping triangle corner."),
					       MeshLocalVertexIndex, FaceIdx / 3, *Mesh.Name);
					Wedge.VertexIndex = TotalVertexOffset;
					Wedge.MatIndex = LocalMaterialIndex;
					Wedge.UVs[0] = FVector2f(0.f, 0.f);
					Wedge.Color = FColor::White;
					Triangle.WedgeIndex[Corner] = WedgeGlobalIndex;
					continue;
				}

				Wedge.VertexIndex = MeshLocalVertexIndex + TotalVertexOffset;
				Wedge.MatIndex = LocalMaterialIndex;

				// UVs
				if (Mesh.VertexUV.IsValidIndex(MeshLocalVertexIndex))
				{
					Wedge.UVs[0] = FVector2f(Mesh.VertexUV[MeshLocalVertexIndex].X,
					                         Mesh.VertexUV[MeshLocalVertexIndex].Y);
				}
				else
				{
					Wedge.UVs[0] = FVector2f(0.f, 0.f);
				}
				for (int32 TexCoordIdx = 1; TexCoordIdx < MAX_TEXCOORDS; ++TexCoordIdx)
				{
					Wedge.UVs[TexCoordIdx] = FVector2f(0.f, 0.f);
				}

				// Colors
				if (Mesh.VertexColor.IsValidIndex(MeshLocalVertexIndex))
				{
					Wedge.Color = FColor(
						(Mesh.VertexColor[MeshLocalVertexIndex] >> 16) & 0xFF,
						(Mesh.VertexColor[MeshLocalVertexIndex] >> 8) & 0xFF,
						(Mesh.VertexColor[MeshLocalVertexIndex] >> 0) & 0xFF,
						(Mesh.VertexColor[MeshLocalVertexIndex] >> 24) & 0xFF);
					bLodHasColors = true;
				}
				else
				{
					Wedge.Color = FColor(255, 255, 255, 255);
				}

				// Normals & Tangents
				if (Mesh.VertexNormals.IsValidIndex(MeshLocalVertexIndex))
				{
					Triangle.TangentZ[Corner] = FVector3f(
						Mesh.VertexNormals[MeshLocalVertexIndex].X,
						-Mesh.VertexNormals[MeshLocalVertexIndex].Y,
						Mesh.VertexNormals[MeshLocalVertexIndex].Z).GetSafeNormal();
					bLodHasNormals = true;
				}
				if (Mesh.VertexTangents.IsValidIndex(MeshLocalVertexIndex))
				{
					Triangle.TangentX[Corner] = FVector3f(
						Mesh.VertexTangents[MeshLocalVertexIndex].X,
						-Mesh.VertexTangents[MeshLocalVertexIndex].Y,
						Mesh.VertexTangents[MeshLocalVertexIndex].Z).GetSafeNormal();
					bLodHasTangents = true;
				}

				Triangle.WedgeIndex[Corner] = WedgeGlobalIndex;
			}
		}

		// Influences (Weights)
		if (!Mesh.VertexWeights.IsEmpty())
		{
			for (int32 VertexId = 0; VertexId < Mesh.VertexWeights.Num(); ++VertexId)
			{
				for (int32 InfluenceIdx = 0; InfluenceIdx < Mesh.VertexWeights[VertexId].WeightCount; ++InfluenceIdx)
				{
					SkeletalMeshImportData::FRawBoneInfluence Influence;
					Influence.VertexIndex = TotalVertexOffset + VertexId;
					Influence.BoneIndex = Mesh.VertexWeights[VertexId].BoneValues[InfluenceIdx];
					Influence.Weight = Mesh.VertexWeights[VertexId].WeightValues[InfluenceIdx];
					OutImportData.Influences.Add(Influence);
				}
			}
		}
		else if (Mesh.MaxWeight > 0)
		{
			for (int32 InfluenceIdx = 0; InfluenceIdx < Mesh.VertexWeightBone.Num(); ++InfluenceIdx)
			{
				int32 LocalVertexIdx = InfluenceIdx / Mesh.MaxWeight;
				if (LocalVertexIdx >= MeshVertexCount)
				{
					UE_LOG(LogCast, Error,
					       TEXT("Weight data index out of bounds for mesh %s. Skipping remaining weights."),
					       *Mesh.Name);
					break;
				}
				int32 GlobalVertexIdx = LocalVertexIdx + TotalVertexOffset;
				int32 BoneGlobalIndex = Mesh.VertexWeightBone[InfluenceIdx];
				float Weight = Mesh.VertexWeightValue[InfluenceIdx];

				if (Weight > SMALL_NUMBER)
				{
					SkeletalMeshImportData::FRawBoneInfluence Influence;
					Influence.VertexIndex = GlobalVertexIdx;
					Influence.BoneIndex = BoneGlobalIndex;
					Influence.Weight = Weight;
					OutImportData.Influences.Add(Influence);
				}
			}
		}

		TotalVertexOffset += MeshVertexCount;
	}

	bOutHasVertexColors |= bLodHasColors;
	MaxUVLayer = FMath::Max(MaxUVLayer, LodMaxUVLayer);

	OutImportData.NumTexCoords = FMath::Max(1, MaxUVLayer + 1);
	OutImportData.bHasVertexColors = bOutHasVertexColors;
	OutImportData.bHasNormals = bLodHasNormals;
	OutImportData.bHasTangents = bLodHasTangents;

	if (OutImportData.Points.IsEmpty() || OutImportData.Faces.IsEmpty())
	{
		UE_LOG(LogCast, Warning, TEXT("Skeletal Mesh Import Data is empty after processing ModelInfo."));
		return false;
	}

	return true;
}

void FDefaultCastMeshImporter::EnsureSkeletonAndPhysicsAsset(USkeletalMesh* SkeletalMesh,
                                                             const FCastImportOptions& Options,
                                                             UObject* ParentPackage,
                                                             TArray<UObject*>& OutCreatedObjects)
{
	if (!SkeletalMesh) return;

	USkeleton* Skeleton = SkeletalMesh->GetSkeleton();
	UPackage* SkelPackage = SkeletalMesh->GetOutermost();

	FString TargetDirectoryPath = ParentPackage->IsA<UPackage>()
		                              ? ParentPackage->GetPathName()
		                              : FPaths::GetPath(ParentPackage->GetPathName());

	if (!Skeleton && Options.Skeleton)
	{
		Skeleton = Options.Skeleton;
		SkeletalMesh->SetSkeleton(Skeleton);
		Skeleton->Modify();
		Skeleton->MergeAllBonesToBoneTree(SkeletalMesh);
		Skeleton->MarkPackageDirty();
	}
	else if (!Skeleton)
	{
		FString SkeletonName = FString::Printf(TEXT("%s_Skeleton"), *SkeletalMesh->GetName());
		UE_LOG(LogCast, Log, TEXT("Creating new skeleton %s for mesh %s"), *SkeletonName, *SkeletalMesh->GetName());

		Skeleton = CreateAsset<USkeleton>(TargetDirectoryPath, SkeletonName, true);
		if (Skeleton)
		{
			OutCreatedObjects.Add(Skeleton);
			Skeleton->Modify();
			Skeleton->MergeAllBonesToBoneTree(SkeletalMesh);
			SkeletalMesh->SetSkeleton(Skeleton);
			Skeleton->MarkPackageDirty();
		}
		else
		{
			UE_LOG(LogCast, Error, TEXT("Failed to create Skeleton for %s"), *SkeletalMesh->GetName());
		}
	}
	else
	{
		// Skeleton already exists, ensure it's compatible
		UE_LOG(LogCast, Log, TEXT("Mesh %s already has skeleton %s. Merging bones."), *SkeletalMesh->GetName(),
		       *Skeleton->GetName());
		Skeleton->Modify();
		Skeleton->MergeAllBonesToBoneTree(SkeletalMesh);
		Skeleton->MarkPackageDirty();
	}


	// --- Physics Asset ---
	UPhysicsAsset* PhysicsAsset = SkeletalMesh->GetPhysicsAsset();

	if (!PhysicsAsset && Options.PhysicsAsset)
	{
		UE_LOG(LogCast, Log, TEXT("Assigning provided PhysicsAsset %s to mesh %s"), *Options.PhysicsAsset->GetName(),
		       *SkeletalMesh->GetName());
		SkeletalMesh->SetPhysicsAsset(Options.PhysicsAsset);
		SkeletalMesh->MarkPackageDirty();
	}
	else if (!PhysicsAsset)
	{
		FString PhysicsAssetName = FString::Printf(TEXT("%s_PhysicsAsset"), *SkeletalMesh->GetName());
		UE_LOG(LogCast, Log, TEXT("Creating new PhysicsAsset %s for mesh %s"), *PhysicsAssetName,
		       *SkeletalMesh->GetName());

		UPhysicsAsset* NewPhysicsAsset = CreateAsset<UPhysicsAsset>(TargetDirectoryPath, PhysicsAssetName, true);
		if (NewPhysicsAsset)
		{
			OutCreatedObjects.Add(NewPhysicsAsset);
			FPhysAssetCreateParams NewBodyData;
			// NewBodyData.MinBoneSize = 10.f;

			FText CreationErrorMessage;
			bool bSuccess = FPhysicsAssetUtils::CreateFromSkeletalMesh(NewPhysicsAsset, SkeletalMesh, NewBodyData,
			                                                           CreationErrorMessage);

			if (bSuccess)
			{
				SkeletalMesh->SetPhysicsAsset(NewPhysicsAsset);
				NewPhysicsAsset->MarkPackageDirty();
				SkeletalMesh->MarkPackageDirty();
			}
			else
			{
				UE_LOG(LogCast, Error, TEXT("Failed to create PhysicsAsset for %s: %s"), *SkeletalMesh->GetName(),
				       *CreationErrorMessage.ToString());
				ObjectTools::DeleteSingleObject(NewPhysicsAsset);
			}
		}
		else
		{
			UE_LOG(LogCast, Error, TEXT("Failed to create PhysicsAsset object for %s"), *SkeletalMesh->GetName());
		}
	}
	else
	{
		UE_LOG(LogCast, Log, TEXT("Mesh %s already has PhysicsAsset %s."), *SkeletalMesh->GetName(),
		       *PhysicsAsset->GetName());
	}
}
