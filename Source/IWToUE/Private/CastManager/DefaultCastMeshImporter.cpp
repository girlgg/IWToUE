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
	if (!MaterialImporter)
	{
		UE_LOG(LogCast, Error, TEXT("Cannot import static mesh: Material Importer is null."));
		return nullptr;
	}
	if (CastScene.Roots.IsEmpty())
	{
		UE_LOG(LogCast, Warning, TEXT("Cannot import static mesh '%s': No mesh data provided in ModelInfo."),
		       *Name.ToString());
		return nullptr;
	}
	// Perhaps there are multiple models?
	FCastRoot& Root = CastScene.Roots[0];
	if (Root.ModelLodInfo.IsEmpty())
	{
		if (Root.Models.IsEmpty())
		{
			UE_LOG(LogCast, Error, TEXT("Cannot import static mesh '%s': No ModelLodInfo and no Models found."),
			       *Name.ToString());
			return nullptr;
		}
		// Add a default LOD entry pointing to the first model if none exists
		Root.ModelLodInfo.AddDefaulted();
		Root.ModelLodInfo[0].ModelIndex = 0;
		Root.ModelLodInfo[0].Distance = 0.0f;
		UE_LOG(LogCast, Warning,
		       TEXT("No ModelLodInfo found for '%s'. Creating default LOD0 entry pointing to Models[0]."),
		       *Name.ToString());
	}
	// --- Prepare Asset Creation ---
	UE_LOG(LogCast, Log, TEXT("Starting static mesh import for: %s with LOD support"), *Name.ToString());

	FString MeshName = NoIllegalSigns(Name.ToString());
	FString ParentPackagePath = InParent->IsA<UPackage>()
		                            ? InParent->GetPathName()
		                            : FPaths::GetPath(InParent->GetPathName());

	// --- Create Static Mesh Asset ---
	UStaticMesh* StaticMesh = CreateAsset<UStaticMesh>(ParentPackagePath, MeshName, true);
	if (!StaticMesh)
	{
		UE_LOG(LogCast, Error, TEXT("Failed to create UStaticMesh asset object: %s in package %s"), *MeshName,
		       *ParentPackagePath);
		return nullptr;
	}
	StaticMesh->PreEditChange(nullptr);

	// --- Shared Data Across LODs ---
	TMap<uint32, TPair<FName, UMaterialInterface*>> SharedMaterialMap;
	TArray<FName> UniqueMaterialSlotNames;
	bool bHasNormals_AnyLOD = false;
	bool bHasTangents_AnyLOD = false;
	int MaxUVChannels_AnyLOD = 1;

	StaticMesh->SetNumSourceModels(0);

	// --- Process Each LOD ---
	for (int32 LodIndex = 0; LodIndex < Root.ModelLodInfo.Num(); ++LodIndex)
	{
		const FCastModelLod& LodInfo = Root.ModelLodInfo[LodIndex];

		if (!Root.Models.IsValidIndex(LodInfo.ModelIndex))
		{
			UE_LOG(LogCast, Error, TEXT("Invalid ModelIndex %d specified for LOD %d in '%s'. Skipping this LOD."),
			       LodInfo.ModelIndex, LodIndex, *MeshName);
			continue;
		}

		const FCastModelInfo& ModelInfo = Root.Models[LodInfo.ModelIndex];
		if (ModelInfo.Meshes.IsEmpty())
		{
			UE_LOG(LogCast, Warning,
			       TEXT("ModelInfo at index %d (for LOD %d) has no mesh data in '%s'. Skipping this LOD."),
			       LodInfo.ModelIndex, LodIndex, *MeshName);
			continue;
		}

		UE_LOG(LogCast, Log, TEXT("Processing LOD %d using ModelIndex %d for %s"), LodIndex, LodInfo.ModelIndex,
		       *MeshName);

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
			       TEXT("Failed to populate mesh description or result was empty for LOD %d in %s. Skipping this LOD."),
			       LodIndex, *MeshName);
			continue;
		}

		FStaticMeshAttributes Attributes(MeshDescription);
		if (Attributes.GetVertexInstanceNormals().IsValid() && Attributes.GetVertexInstanceNormals().GetNumElements() >
			0)
			bHasNormals_AnyLOD = true;
		if (Attributes.GetVertexInstanceTangents().IsValid() && Attributes.GetVertexInstanceTangents().GetNumElements()
			> 0)
			bHasTangents_AnyLOD = true;
		MaxUVChannels_AnyLOD = FMath::Max(MaxUVChannels_AnyLOD, Attributes.GetVertexInstanceUVs().GetNumChannels());

		FStaticMeshSourceModel& SrcModel = StaticMesh->AddSourceModel();

		// --- Configure Build Settings ---
		SrcModel.BuildSettings.bRecomputeNormals = !bHasNormals_AnyLOD;
		SrcModel.BuildSettings.bRecomputeTangents = !bHasTangents_AnyLOD || !bHasNormals_AnyLOD;
		SrcModel.BuildSettings.bUseMikkTSpace = SrcModel.BuildSettings.bRecomputeTangents;
		SrcModel.BuildSettings.bGenerateLightmapUVs = (LodIndex == 0);
		SrcModel.BuildSettings.SrcLightmapIndex = 0;
		SrcModel.BuildSettings.DstLightmapIndex = MaxUVChannels_AnyLOD;
		SrcModel.BuildSettings.bRemoveDegenerates = true;
		SrcModel.BuildSettings.bUseHighPrecisionTangentBasis = false;
		SrcModel.BuildSettings.bUseFullPrecisionUVs = false;

		// Set Screen Size for this LOD
		// Map distance to screen size (larger distance -> smaller screen size)
		// float ScreenSize = FMath::Clamp(10 / FMath::Max(1.0f, LodInfo.Distance), 0.01f, 1.0f);
		// SrcModel.ScreenSize.Default = ScreenSize;
		// UE_LOG(LogCast, Log, TEXT("  LOD %d ScreenSize set to: %f"), LodIndex, SrcModel.ScreenSize.Default);

		FMeshDescription* StaticMeshDescription = StaticMesh->CreateMeshDescription(LodIndex);
		if (StaticMeshDescription)
		{
			*StaticMeshDescription = MeshDescription;
			StaticMesh->CommitMeshDescription(LodIndex);
			UE_LOG(LogCast, Log, TEXT("Committed MeshDescription for LOD %d to StaticMesh %s"), LodIndex, *MeshName);
		}
		else
		{
			UE_LOG(LogCast, Error, TEXT("Failed to get or create mesh description for LOD %d on %s"), LodIndex,
			       *MeshName);
		}
	}

	if (StaticMesh->GetNumSourceModels() == 0)
	{
		UE_LOG(LogCast, Error, TEXT("No valid LODs were imported for %s. Aborting."), *MeshName);
		StaticMesh->MarkAsGarbage();
		return nullptr;
	}

	StaticMesh->InitResources();
	StaticMesh->SetLightingGuid();

	TArray<FStaticMaterial> StaticMaterials;

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
			UE_LOG(LogCast, Error, TEXT("Could not find material for slot name %s during final material setup for %s."),
			       *SlotName.ToString(), *MeshName);
			StaticMaterials.Add(FStaticMaterial(UMaterial::GetDefaultMaterial(MD_Surface), SlotName, SlotName));
		}
	}
	StaticMesh->SetStaticMaterials(StaticMaterials);

	StaticMesh->GetSectionInfoMap().Clear();
	StaticMesh->GetOriginalSectionInfoMap().Clear();
	for (int32 LodIdx = 0; LodIdx < StaticMesh->GetNumSourceModels(); ++LodIdx)
	{
		const FMeshDescription* CommittedMeshDesc = StaticMesh->GetMeshDescription(LodIdx);
		if (!CommittedMeshDesc) continue;

		TPolygonGroupAttributesConstRef<FName> PolygonGroupMaterialSlotNames =
			CommittedMeshDesc->PolygonGroupAttributes().GetAttributesRef<FName>(
				MeshAttribute::PolygonGroup::ImportedMaterialSlotName);

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
				UE_LOG(LogCast, Error, TEXT("Failed to find MaterialIndex for SlotName '%s' in LOD %d, Section %d."),
				       *SlotName.ToString(), LodIdx, SectionIndex);
				MaterialIndex = 0; // Fallback to material 0
			}

			FMeshSectionInfo Info;
			Info.MaterialIndex = MaterialIndex;
			Info.bCastShadow = true; // Default values
			Info.bEnableCollision = true;
			StaticMesh->GetSectionInfoMap().Set(LodIdx, SectionIndex, Info);
			SectionIndex++;
		}
	}
	StaticMesh->GetOriginalSectionInfoMap().CopyFrom(StaticMesh->GetSectionInfoMap());

	// --- Build and Finalize ---
	UE_LOG(LogCast, Log, TEXT("Building StaticMesh %s..."), *MeshName);
	StaticMesh->Build(false); // Build the mesh with all LODs

	// --- Asset Import Data ---
	if (!OriginalFilePath.IsEmpty())
	{
		if (UAssetImportData* ImportDataPtr = StaticMesh->GetAssetImportData())
		{
			FString FullPath = FPaths::ConvertRelativePathToFull(OriginalFilePath);
			ImportDataPtr->Update(FullPath);
		}
		else
		{
			UAssetImportData* NewImportData = NewObject<UAssetImportData>(StaticMesh, TEXT("AssetImportData"));
			StaticMesh->SetAssetImportData(NewImportData);
			FString FullPath = FPaths::ConvertRelativePathToFull(OriginalFilePath);
			NewImportData->Update(FullPath);
		}
	}

	StaticMesh->PostEditChange();
	StaticMesh->MarkPackageDirty();

	UE_LOG(LogCast, Log, TEXT("Successfully imported Static Mesh: %s"), *StaticMesh->GetPathName());

	return StaticMesh;
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
	if (CastScene.Roots.IsEmpty())
	{
		UE_LOG(LogCast, Error, TEXT("Cannot import skeletal mesh: No CastRoot data provided."));
		return nullptr;
	}

	if (!MaterialImporter)
	{
		UE_LOG(LogCast, Error, TEXT("Cannot import skeletal mesh: Material Importer is null."));
		return nullptr;
	}
	// Perhaps there are multiple models?
	FCastRoot& Root = CastScene.Roots[0];

	if (Root.ModelLodInfo.IsEmpty())
	{
		if (Root.Models.IsEmpty())
		{
			UE_LOG(LogCast, Error, TEXT("Cannot import skeletal mesh '%s': No ModelLodInfo and no Models found."),
			       *Name.ToString());
			return nullptr;
		}
		Root.ModelLodInfo.AddDefaulted();
		Root.ModelLodInfo[0].ModelIndex = 0;
		Root.ModelLodInfo[0].Distance = 0.0f;
		UE_LOG(LogCast, Warning,
		       TEXT("No ModelLodInfo found for '%s'. Creating default LOD0 entry pointing to Models[0]."),
		       *Name.ToString());
	}

	// --- Find Base Model for Skeleton/Bones ---
	int32 BaseModelIndex = -1;
	const FCastModelInfo* BaseModelInfoPtr = nullptr;
	if (Root.ModelLodInfo.Num() > 0 && Root.Models.IsValidIndex(Root.ModelLodInfo[0].ModelIndex))
	{
		BaseModelIndex = Root.ModelLodInfo[0].ModelIndex;
		BaseModelInfoPtr = &Root.Models[BaseModelIndex];
	}
	else if (Root.Models.Num() > 0) // Fallback if LOD info is bad/missing
	{
		BaseModelIndex = 0;
		BaseModelInfoPtr = &Root.Models[0];
		UE_LOG(LogCast, Warning,
		       TEXT("Could not find valid base model from ModelLodInfo[0]. Using Models[0] for skeleton definition."));
	}

	if (!BaseModelInfoPtr)
	{
		UE_LOG(LogCast, Error, TEXT("Cannot find a valid base FCastModelInfo to define the skeleton for %s."),
		       *Name.ToString());
		return nullptr;
	}
	const FCastModelInfo& BaseModelInfo = *BaseModelInfoPtr;

	FScopedSlowTask SlowTask(100, NSLOCTEXT("CastImporter", "ImportingSkeletalMesh", "Importing Skeletal Mesh..."));
	SlowTask.MakeDialog();

	FString MeshName = NoIllegalSigns(Name.ToString());
	FString ParentPackagePath =
		InParent->IsA<UPackage>() ? InParent->GetPathName() : FPaths::GetPath(InParent->GetPathName());

	SlowTask.EnterProgressFrame(5, FText::Format(
		                            NSLOCTEXT("CastImporter", "SkM_CreatingAsset", "Creating Asset {0}"),
		                            FText::FromString(MeshName)));
	USkeletalMesh* SkeletalMesh = CreateAsset<USkeletalMesh>(ParentPackagePath, MeshName, true);
	if (!SkeletalMesh)
	{
		UE_LOG(LogCast, Error, TEXT("Failed to create USkeletalMesh asset object: %s"), *MeshName);
		return nullptr;
	}

	// --- Shared Data Across LODs ---
	TMap<uint32, int32> SharedMaterialMap; // Cast Material Hash -> Index in FinalMaterials array
	TArray<SkeletalMeshImportData::FMaterial> FinalMaterials; // The single list of materials for the asset
	TArray<SkeletalMeshImportData::FBone> RefBonesBinary; // Populated ONCE from the base model
	bool bHasVertexColors_AnyLOD = false;
	int MaxUVLayer_AnyLOD = 0;
	TArray<FVector> AllMeshPoints_LOD0; // Use LOD0 points for initial bounds calculation

	// --- Process Skeleton from Base Model ---
	SlowTask.EnterProgressFrame(10, NSLOCTEXT("CastImporter", "SkM_ProcessingSkeleton", "Processing Skeleton..."));
	int32 TotalBoneOffset = 0;
	for (const FCastSkeletonInfo& Skeleton : BaseModelInfo.Skeletons)
	{
		for (const FCastBoneInfo& Bone : Skeleton.Bones)
		{
			SkeletalMeshImportData::FBone BoneBinary;
			BoneBinary.Name = Bone.BoneName;
			BoneBinary.ParentIndex = Bone.ParentIndex == -1 ? INDEX_NONE : Bone.ParentIndex + TotalBoneOffset;
			BoneBinary.NumChildren = 0;

			SkeletalMeshImportData::FJointPos& JointMatrix = BoneBinary.BonePos;
			JointMatrix.Transform.SetTranslation(
				FVector3f(Bone.LocalPosition.X, -Bone.LocalPosition.Y, Bone.LocalPosition.Z));
			FQuat4f RawBoneRotator(Bone.LocalRotation.X,
			                       -Bone.LocalRotation.Y,
			                       Bone.LocalRotation.Z,
			                       -Bone.LocalRotation.W);
			JointMatrix.Transform.SetRotation(RawBoneRotator);
			JointMatrix.Transform.SetScale3D(FVector3f(Bone.Scale));

			RefBonesBinary.Add(BoneBinary);
		}
		TotalBoneOffset += Skeleton.Bones.Num();
	}
	for (int32 BoneIdx = 0; BoneIdx < RefBonesBinary.Num(); ++BoneIdx)
	{
		const int32 ParentIdx = RefBonesBinary[BoneIdx].ParentIndex;
		if (ParentIdx != INDEX_NONE && RefBonesBinary.IsValidIndex(ParentIdx))
		{
			RefBonesBinary[ParentIdx].NumChildren++;
		}
	}

	// --- Setup Skeletal Mesh Asset Base ---
	SkeletalMesh->PreEditChange(nullptr);
	SkeletalMesh->InvalidateDeriveDataCacheGUID();
	SkeletalMesh->ResetLODInfo();

	FSkeletalMeshModel* ImportedModel = SkeletalMesh->GetImportedModel();
	check(ImportedModel);
	ImportedModel->LODModels.Empty();

	// --- Process Each LOD ---
	float ProgressPerLOD = 40.0f / FMath::Max(1, Root.ModelLodInfo.Num());
	for (int32 LodIndex = 0; LodIndex < Root.ModelLodInfo.Num(); ++LodIndex)
	{
		const FCastModelLod& LodInfo = Root.ModelLodInfo[LodIndex];
		SlowTask.EnterProgressFrame(ProgressPerLOD,
		                            FText::Format(
			                            NSLOCTEXT("CastImporter", "SkM_ProcessingLOD", "Processing LOD {0}..."),
			                            FText::AsNumber(LodIndex)));

		if (!Root.Models.IsValidIndex(LodInfo.ModelIndex))
		{
			UE_LOG(LogCast, Error, TEXT("Invalid ModelIndex %d specified for LOD %d in '%s'. Skipping this LOD."),
			       LodInfo.ModelIndex, LodIndex, *MeshName);
			continue;
		}

		const FCastModelInfo& CurrentLodModelInfo = Root.Models[LodInfo.ModelIndex];
		if (CurrentLodModelInfo.Meshes.IsEmpty())
		{
			UE_LOG(LogCast, Warning,
			       TEXT("ModelInfo at index %d (for LOD %d) has no mesh data in '%s'. Skipping this LOD."),
			       LodInfo.ModelIndex, LodIndex, *MeshName);
			continue;
		}

		UE_LOG(LogCast, Log, TEXT("Processing LOD %d using ModelIndex %d for %s"), LodIndex, LodInfo.ModelIndex,
		       *MeshName);

		FSkeletalMeshImportData LodImportData;
		LodImportData.RefBonesBinary = RefBonesBinary;

		bool bPopulateSuccess = PopulateSkeletalMeshImportData(
			Root,
			CurrentLodModelInfo,
			LodImportData,
			Options,
			MaterialImporter,
			SkeletalMesh,
			SharedMaterialMap,
			FinalMaterials,
			OutCreatedObjects,
			bHasVertexColors_AnyLOD,
			MaxUVLayer_AnyLOD);

		if (!bPopulateSuccess || LodImportData.Points.IsEmpty() || LodImportData.Faces.IsEmpty())
		{
			UE_LOG(LogCast, Error,
			       TEXT(
				       "Failed to populate skeletal mesh import data or result was empty for LOD %d in %s. Skipping this LOD."
			       ), LodIndex, *MeshName);
			continue;
		}

		// Store points from LOD0 for bounding box calculation
		if (LodIndex == 0)
		{
			AllMeshPoints_LOD0.Reserve(LodImportData.Points.Num());
			for (const FVector3f& Pt : LodImportData.Points) AllMeshPoints_LOD0.Add(FVector(Pt)); // Convert to FVector
		}

		// Add LOD Info entry to the Skeletal Mesh Asset
		FSkeletalMeshLODInfo& AssetLODInfo = SkeletalMesh->AddLODInfo();
		AssetLODInfo.ScreenSize = LodInfo.Distance / 10;
		UE_LOG(LogCast, Log, TEXT("  LOD %d ScreenSize set to: %f"), LodIndex, LodInfo.Distance / 10);

		FSkeletalMeshBuildSettings BuildOptions;
		BuildOptions.bRecomputeNormals = !LodImportData.bHasNormals;
		BuildOptions.bRecomputeTangents = !LodImportData.bHasTangents;
		BuildOptions.bUseMikkTSpace = BuildOptions.bRecomputeTangents;
		BuildOptions.bComputeWeightedNormals = true;
		BuildOptions.bRemoveDegenerates = true;
		BuildOptions.bUseHighPrecisionTangentBasis = false;
		BuildOptions.bUseFullPrecisionUVs = false;
		AssetLODInfo.BuildSettings = BuildOptions;

		// Add a new LODModel entry to store the import data
		ImportedModel->LODModels.Add(new FSkeletalMeshLODModel());
		FSkeletalMeshLODModel& LODModel = ImportedModel->LODModels[LodIndex];
		LODModel.NumTexCoords = FMath::Max(1, MaxUVLayer_AnyLOD);

		// Save the populated import data to this specific LOD index
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		SkeletalMesh->SaveLODImportedData(LodIndex, LodImportData);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	// Check using LODNum which reflects added LODInfo
	if (SkeletalMesh->GetLODNum() == 0)
	{
		UE_LOG(LogCast, Error, TEXT("No valid LODs were imported for Skeletal Mesh %s. Aborting."), *MeshName);
		SkeletalMesh->MarkAsGarbage();
		return nullptr;
	}

	// --- Finalize Skeletal Mesh Asset ---
	SlowTask.EnterProgressFrame(5, NSLOCTEXT("CastImporter", "SkM_SetupRefSkel", "Setting Up Reference Skeleton..."));

	TArray<FSkeletalMaterial>& MeshMaterials = SkeletalMesh->GetMaterials();
	MeshMaterials.Empty(FinalMaterials.Num());
	for (const auto& MatData : FinalMaterials)
	{
		MeshMaterials.Add(FSkeletalMaterial(MatData.Material.Get(), true, false, FName(*MatData.MaterialImportName),
		                                    FName(*MatData.MaterialImportName)));
	}

	// RTTI, when exiting the scope, FReferenceSkeletonModifier calls the destructor to complete the skeleton construction.
	{
		FReferenceSkeletonModifier RefSkelModifier(SkeletalMesh->GetRefSkeleton(), SkeletalMesh->GetSkeleton());
		for (const auto& BoneData : RefBonesBinary)
		{
			FName BoneFName = FName(BoneData.Name);
			if (BoneFName.IsNone())
			{
				UE_LOG(LogCast, Warning, TEXT("Skipping bone with invalid name: '%s' during RefSkeleton setup."),
				       *BoneData.Name);
				continue;
			}
			FMeshBoneInfo BoneInfo(BoneFName, BoneData.Name, BoneData.ParentIndex);
			FTransform BoneTransform(BoneData.BonePos.Transform);
			RefSkelModifier.Add(BoneInfo, BoneTransform);
		}
	}

	// --- Setup Skeletal Mesh Asset ---
	SlowTask.EnterProgressFrame(10, NSLOCTEXT("CastImporter", "SkM_SetupAsset", "Setting Up Asset..."));

	// Set final properties determined during import
	SkeletalMesh->SetHasVertexColors(bHasVertexColors_AnyLOD);
	SkeletalMesh->SetVertexColorGuid(bHasVertexColors_AnyLOD ? FGuid::NewGuid() : FGuid());

	// Calculate and set bounds using LOD0 points
	FBox BoundingBox(AllMeshPoints_LOD0.GetData(), AllMeshPoints_LOD0.Num());
	// Calculate from aggregated points of LOD0
	SkeletalMesh->SetImportedBounds(FBoxSphereBounds(BoundingBox));

	// --- Build Mesh ---
	SlowTask.EnterProgressFrame(15, NSLOCTEXT("CastImporter", "SkM_BuildingMeshLODs", "Building Mesh LODs..."));
	IMeshBuilderModule& MeshBuilderModule = FModuleManager::LoadModuleChecked<IMeshBuilderModule>("MeshBuilder");

	SkeletalMesh->Build();

	SlowTask.EnterProgressFrame(2, NSLOCTEXT("CastImporter", "SkM_VerifyRenderData", "Verifying Render Data..."));
	FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetResourceForRendering();
	if (!RenderData || !RenderData->LODRenderData.IsValidIndex(0) || RenderData->LODRenderData[0].GetNumVertices() == 0)
	{
		UE_LOG(LogCast, Error,
		       TEXT(
			       "Failed to generate valid RenderData for %s after building LODs. Build process might have failed internally."
		       ), *MeshName);
		SkeletalMesh->MarkAsGarbage();
		return nullptr;
	}

	// Add asset import data link
	if (!OriginalFilePath.IsEmpty())
	{
		if (UAssetImportData* ImportDataPtr = SkeletalMesh->GetAssetImportData())
		{
			FString FullPath = FPaths::ConvertRelativePathToFull(OriginalFilePath);
			ImportDataPtr->Update(FullPath);
		}
		else
		{
			UAssetImportData* NewImportData = NewObject<UAssetImportData>(SkeletalMesh, TEXT("AssetImportData"));
			SkeletalMesh->SetAssetImportData(NewImportData);
			FString FullPath = FPaths::ConvertRelativePathToFull(OriginalFilePath);
			NewImportData->Update(FullPath);
		}
	}

	// Finalize
	SlowTask.EnterProgressFrame(3, NSLOCTEXT("CastImporter", "SkM_Finalizing", "Finalizing..."));
	SkeletalMesh->CalculateInvRefMatrices();

	EnsureSkeletonAndPhysicsAsset(SkeletalMesh, Options, InParent, OutCreatedObjects);

	SkeletalMesh->PostEditChange();
	SkeletalMesh->MarkPackageDirty();

	UE_LOG(LogCast, Log, TEXT("Successfully imported Skeletal Mesh with %d LODs: %s"), SkeletalMesh->GetLODNum(),
	       *SkeletalMesh->GetPathName());

	return SkeletalMesh;
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
		if (int32* FoundLocalIndex = LodMaterialMap.Find(Mesh.MaterialHash))
		{
			LocalMaterialIndex = *FoundLocalIndex;
		}
		else
		{
			int32 FinalMaterialIndex = INDEX_NONE;
			UMaterialInterface* UnrealMaterial = nullptr;
			FString MaterialImportName;

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
					MaterialImportName = CastMaterial.Name;
					UnrealMaterial = MaterialImporter->CreateMaterialInstance(CastMaterial, Options, SkeletalMesh);
					if (!UnrealMaterial)
					{
						UE_LOG(LogCast, Warning,
						       TEXT("Failed to create material instance %s for mesh %s (Hash: %llu). Using default."),
						       *MaterialImportName, *Mesh.Name, Mesh.MaterialHash);
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
						       "Invalid MaterialIndex %d or hash mismatch for MaterialHash %llu on mesh %s. Check pre-processing. Using default material."
					       ),
					       Mesh.MaterialIndex, Mesh.MaterialHash, *Mesh.Name);
					UnrealMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
					MaterialImportName = FString::Printf(TEXT("DefaultMaterial_%llu"), Mesh.MaterialHash);
				}

				SkeletalMeshImportData::FMaterial NewFinalMaterialData;
				NewFinalMaterialData.Material = UnrealMaterial;
				NewFinalMaterialData.MaterialImportName = MaterialImportName;
				FinalMaterialIndex = FinalMaterials.Add(NewFinalMaterialData);

				SharedMaterialMap.Add(Mesh.MaterialHash, FinalMaterialIndex);
			}

			if (UnrealMaterial != nullptr)
			{
				SkeletalMeshImportData::FMaterial NewLocalMaterialData;
				NewLocalMaterialData.Material = UnrealMaterial;
				NewLocalMaterialData.MaterialImportName = MaterialImportName;
				LocalMaterialIndex = OutImportData.Materials.Add(NewLocalMaterialData);
			}
			else
			{
				UE_LOG(LogCast, Error, TEXT("Failed to resolve UnrealMaterial for hash %llu. Assigning local index 0."),
				       Mesh.MaterialHash);
				if (OutImportData.Materials.IsEmpty())
				{
					SkeletalMeshImportData::FMaterial DefaultMatData;
					DefaultMatData.Material = UMaterial::GetDefaultMaterial(MD_Surface);
					DefaultMatData.MaterialImportName = TEXT("Default");
					OutImportData.Materials.Add(DefaultMatData);
				}
				LocalMaterialIndex = 0;
			}

			LodMaterialMap.Add(Mesh.MaterialHash, LocalMaterialIndex);
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
