#include "Importers/ModelImporter.h"

#include "CastManager/CastImportOptions.h"
#include "CastManager/CastRoot.h"
#include "CastManager/DefaultCastMaterialImporter.h"
#include "CastManager/DefaultCastMeshImporter.h"
#include "Interface/IGameAssetHandler.h"
#include "Utils/CoDAssetHelper.h"
#include "WraithX/CoDAssetType.h"

FModelImporter::FModelImporter()
{
	MeshImporter = MakeShared<FDefaultCastMeshImporter>();
	MaterialImporterInternal = MakeShared<FDefaultCastMaterialImporter>();
}

bool FModelImporter::Import(const FAssetImportContext& Context, TArray<UObject*>& OutCreatedObjects,
                            FOnAssetImportProgress ProgressDelegate)
{
	TSharedPtr<FCoDModel> ModelInfo = Context.GetAssetInfo<FCoDModel>();
	if (!ModelInfo.IsValid() || !Context.GameHandler || !Context.ImportManager || !Context.AssetCache || !MeshImporter.
		IsValid() || !MaterialImporterInternal.IsValid())
	{
		UE_LOG(LogITUAssetImporter, Error, TEXT("Model Import failed: Invalid context parameters. Asset: %s"),
		       Context.SourceAsset.IsValid() ? *Context.SourceAsset->AssetName : TEXT("Unknown"));
		ProgressDelegate.ExecuteIfBound(1.0f, NSLOCTEXT("ModelImporter", "ErrorInvalidContext",
		                                                "Import Failed: Invalid Context"));
		return false;
	}

	uint64 AssetKey = ModelInfo->AssetPointer; // Use pointer as key for model cache
	FString OriginalAssetName = FPaths::GetBaseFilename(ModelInfo->AssetName);
	FString SanitizedAssetName = FCoDAssetNameHelper::NoIllegalSigns(OriginalAssetName);
	if (SanitizedAssetName.IsEmpty())
	{
		SanitizedAssetName = FString::Printf(TEXT("XModel_ptr_%llx"), AssetKey);
	}

	ProgressDelegate.ExecuteIfBound(0.0f, FText::Format(
		                                NSLOCTEXT("ModelImporter", "Start", "Starting Import: {0}"),
		                                FText::FromString(SanitizedAssetName)));

	// --- 1. Check Model Cache ---
	{
		FScopeLock Lock(&Context.AssetCache->CacheMutex);
		if (TWeakObjectPtr<UObject>* Found = Context.AssetCache->ImportedModels.Find(AssetKey))
		{
			if (Found->IsValid())
			{
				UObject* CachedModel = Found->Get();
				UE_LOG(LogITUAssetImporter, Log, TEXT("Model '%s' (Key: 0x%llX) found in cache: %s. Skipping import."),
				       *SanitizedAssetName, AssetKey, *CachedModel->GetPathName());
				OutCreatedObjects.Add(CachedModel);
				ProgressDelegate.ExecuteIfBound(
					1.0f, NSLOCTEXT("ModelImporter", "LoadedFromCache", "Loaded From Cache"));
				return true;
			}
			UE_LOG(LogITUAssetImporter, Verbose,
			       TEXT("Model '%s' (Key: 0x%llX) was cached but is invalid/stale. Re-importing."),
			       *SanitizedAssetName, AssetKey);
			Context.AssetCache->ImportedModels.Remove(AssetKey);
		}
	}

	ProgressDelegate.ExecuteIfBound(0.05f, NSLOCTEXT("ModelImporter", "ReadingModelData", "Reading Model Data..."));

	// --- 2. Read Generic Model Data ---
	FWraithXModel GenericModelData;
	if (!Context.GameHandler->ReadModelData(ModelInfo, GenericModelData))
	{
		UE_LOG(LogITUAssetImporter, Error, TEXT("Failed to read model data for %s (Key: 0x%llX)."), *SanitizedAssetName,
		       AssetKey);
		ProgressDelegate.ExecuteIfBound(1.0f, NSLOCTEXT("ModelImporter", "ErrorReadFail",
		                                                "Import Failed: Cannot Read Data"));
		return false;
	}
	GenericModelData.ModelName = SanitizedAssetName;

	ProgressDelegate.ExecuteIfBound(0.1f, NSLOCTEXT("ModelImporter", "ProcessingDeps", "Processing Dependencies..."));

	// --- 3. Process Dependencies (Textures, Build Material List) ---
	FCastRoot SceneRoot;
	if (!ProcessModelDependencies(GenericModelData, SceneRoot, Context, MaterialImporterInternal.Get(), false,
	                              ProgressDelegate))
	{
		UE_LOG(LogITUAssetImporter, Error, TEXT("Failed during dependency processing for model %s."),
		       *GenericModelData.ModelName);
		ProgressDelegate.ExecuteIfBound(1.0f, NSLOCTEXT("ModelImporter", "ErrorDepsFail",
		                                                "Import Failed: Dependency Error"));
		return false;
	}

	ProgressDelegate.ExecuteIfBound(0.4f, NSLOCTEXT("ModelImporter", "TranslatingLODs", "Translating LOD Geometry..."));

	// --- 4. Translate LOD Geometry Data ---
	int32 LodCount = GenericModelData.ModelLods.Num();
	if (LodCount == 0)
	{
		UE_LOG(LogITUAssetImporter, Error, TEXT("Model %s has no LODs after reading data."),
		       *GenericModelData.ModelName);
		ProgressDelegate.
			ExecuteIfBound(1.0f, NSLOCTEXT("ModelImporter", "ErrorNoLODs", "Import Failed: No LODs Found"));
		return false;
	}

	bool bHasValidGeometry = false;
	SceneRoot.Models.Reserve(LodCount);
	SceneRoot.ModelLodInfo.Reserve(LodCount);

	for (int32 LodIdx = 0; LodIdx < LodCount; ++LodIdx)
	{
		FCastModelInfo TranslatedLodModel;
		if (Context.GameHandler->TranslateModel(GenericModelData, LodIdx, TranslatedLodModel, SceneRoot))
		{
			if (!TranslatedLodModel.Meshes.IsEmpty())
			{
				FCastModelLod ModelLodInfo;
				ModelLodInfo.Distance = GenericModelData.ModelLods[LodIdx].LodDistance;
				ModelLodInfo.MaxDistance = GenericModelData.ModelLods[LodIdx].LodMaxDistance;
				ModelLodInfo.ModelIndex = SceneRoot.Models.Add(MoveTemp(TranslatedLodModel));
				SceneRoot.ModelLodInfo.Add(ModelLodInfo);
				bHasValidGeometry = true;
				UE_LOG(LogITUAssetImporter, Verbose, TEXT("Successfully translated LOD %d for model %s"), LodIdx,
				       *GenericModelData.ModelName);
			}
			else
			{
				UE_LOG(LogITUAssetImporter, Warning,
				       TEXT("TranslateModel for LOD %d of '%s' resulted in empty mesh data."), LodIdx,
				       *GenericModelData.ModelName);
			}
		}
		else
		{
			UE_LOG(LogITUAssetImporter, Warning, TEXT("Failed to translate LOD %d geometry for model %s."), LodIdx,
			       *GenericModelData.ModelName);
		}
	}

	if (!bHasValidGeometry)
	{
		UE_LOG(LogITUAssetImporter, Error,
		       TEXT("No valid geometry was translated for any LOD of model %s. Import failed."),
		       *GenericModelData.ModelName);
		ProgressDelegate.ExecuteIfBound(1.0f, NSLOCTEXT("ModelImporter", "ErrorNoGeom",
		                                                "Import Failed: No Geometry Translated"));
		return false;
	}

	ProgressDelegate.ExecuteIfBound(0.7f, NSLOCTEXT("ModelImporter", "CreatingPackage", "Creating Package..."));

	// --- 5. Create Package ---
	FString PackagePath = FPaths::Combine(Context.BaseImportPath, TEXT("Models"));
	FString FullPackageName = FPaths::Combine(PackagePath, SanitizedAssetName);
	UPackage* Package = CreatePackage(*FullPackageName);
	if (!Package)
	{
		UE_LOG(LogITUAssetImporter, Error, TEXT("Failed to create package: %s"), *FullPackageName);
		ProgressDelegate.ExecuteIfBound(
			1.0f, NSLOCTEXT("ModelImporter", "ErrorPackage", "Import Failed: Package Error"));
		return false;
	}
	Package->FullyLoad();

	ProgressDelegate.ExecuteIfBound(0.75f, NSLOCTEXT("ModelImporter", "ImportingMeshAsset", "Importing Mesh Asset..."));

	// --- 6. Import Mesh using ICastMeshImporter ---
	FCastScene Scene;
	Scene.Roots.Add(MoveTemp(SceneRoot));

	bool bIsSkeletal = Scene.Roots[0].Models.IsValidIndex(Scene.Roots[0].ModelLodInfo[0].ModelIndex) &&
		Scene.Roots[0].Models[Scene.Roots[0].ModelLodInfo[0].ModelIndex].Skeletons.Num() > 0 &&
		Scene.Roots[0].Models[Scene.Roots[0].ModelLodInfo[0].ModelIndex].Skeletons[0].Bones.Num() > 0;


	UObject* ImportedMeshObject = nullptr;
	TArray<UObject*> CreatedMeshDependencies;
	FCastImportOptions Options;

	if (bIsSkeletal)
	{
		UE_LOG(LogITUAssetImporter, Log, TEXT("Importing %s as Skeletal Mesh via ICastMeshImporter."),
		       *SanitizedAssetName);
		ImportedMeshObject = MeshImporter->ImportSkeletalMesh(
			Scene, Options, Package, FName(*SanitizedAssetName), RF_Public | RF_Standalone,
			MaterialImporterInternal.Get(),
			TEXT(""),
			CreatedMeshDependencies);
	}
	else
	{
		UE_LOG(LogITUAssetImporter, Log, TEXT("Importing %s as Static Mesh via ICastMeshImporter."),
		       *SanitizedAssetName);
		ImportedMeshObject = MeshImporter->ImportStaticMesh(
			Scene, Options, Package, FName(*SanitizedAssetName), RF_Public | RF_Standalone,
			MaterialImporterInternal.Get(),
			TEXT(""),
			CreatedMeshDependencies);
	}

	// --- 7. Finalize and Cache ---
	if (ImportedMeshObject)
	{
		UE_LOG(LogITUAssetImporter, Log, TEXT("Successfully imported mesh asset: %s"),
		       *ImportedMeshObject->GetPathName());
		OutCreatedObjects.Add(ImportedMeshObject);
		OutCreatedObjects.Append(CreatedMeshDependencies);

		Context.AssetCache->AddCreatedAsset(ImportedMeshObject);
		Context.AssetCache->AddCreatedAssets(CreatedMeshDependencies); // Includes Skeleton, PhysAsset

		{
			FScopeLock Lock(&Context.AssetCache->CacheMutex);
			Context.AssetCache->ImportedModels.Add(AssetKey, ImportedMeshObject);
		}

		ProgressDelegate.ExecuteIfBound(1.0f, NSLOCTEXT("ModelImporter", "Success", "Import Successful"));
		return true;
	}
	else
	{
		UE_LOG(LogITUAssetImporter, Error, TEXT("ICastMeshImporter failed for %s."), *SanitizedAssetName);
		ProgressDelegate.ExecuteIfBound(1.0f, NSLOCTEXT("ModelImporter", "ErrorImportFail",
		                                                "Import Failed: Mesh Importer Error"));
		return false;
	}
}

UClass* FModelImporter::GetSupportedUClass() const
{
	return UStaticMesh::StaticClass();
}

bool FModelImporter::ProcessModelDependencies(
	FWraithXModel& InOutModelData,
	FCastRoot& OutSceneRoot,
	const FAssetImportContext& Context,
	ICastMaterialImporter* MaterialImporter,
	bool bCreateMaterialInstances,
	const FOnAssetImportProgress& ProgressDelegate)
{
	FImportedAssetsCache& Cache = *Context.AssetCache;
	IGameAssetHandler* Handler = Context.GameHandler;
	constexpr float BaseProgress = 0.1f;
	constexpr float ProgressRange = 0.3f;

	int32 TotalMaterialsProcessed = 0;
	int32 TotalMaterialEntries = 0;
	for (const auto& Lod : InOutModelData.ModelLods) TotalMaterialEntries += Lod.Materials.Num();
	if (TotalMaterialEntries == 0)
	{
		UE_LOG(LogITUAssetImporter, Log, TEXT("Model %s has no materials to process."), *InOutModelData.ModelName);
		ProgressDelegate.ExecuteIfBound(BaseProgress + ProgressRange,
		                                NSLOCTEXT("ModelImporter", "DepsDoneNoMats",
		                                          "Dependencies Processed (No Materials)"));
		return true;
	}

	TMap<uint64, uint32> GlobalMaterialMap;

	FString TexturePackagePath = FPaths::Combine(Context.BaseImportPath, TEXT("Models"), InOutModelData.ModelName,
	                                             TEXT("Materials"), TEXT("Textures"));

	for (FWraithXModelLod& Lod : InOutModelData.ModelLods)
	{
		for (FWraithXMaterial& WraithMaterial : Lod.Materials)
		{
			if (GlobalMaterialMap.Contains(WraithMaterial.MaterialHash))
			{
				continue;
			}

			TotalMaterialsProcessed++;
			const float CurrentProgress =
				BaseProgress + (ProgressRange * (static_cast<float>(TotalMaterialsProcessed) / TotalMaterialEntries));
			FText Status = FText::Format(
				NSLOCTEXT("ModelImporter", "ProcessingMat", "Processing Material {0}... ({1}/{2})"),
				FText::FromString(WraithMaterial.MaterialName.IsEmpty()
					                  ? FString::Printf(TEXT("Hash_%llx"), WraithMaterial.MaterialHash)
					                  : WraithMaterial.MaterialName),
				FText::AsNumber(TotalMaterialsProcessed), FText::AsNumber(TotalMaterialEntries));
			ProgressDelegate.ExecuteIfBound(CurrentProgress, Status);

			// 1. Process Textures for this Material
			for (FWraithXImage& WraithImage : WraithMaterial.Images)
			{
				if (WraithImage.ImagePtr == 0) continue;

				// --- Check Cache ---
				UTexture2D* TextureAsset = nullptr;
				{
					FScopeLock Lock(&Cache.CacheMutex);
					if (TWeakObjectPtr<UTexture2D>* Found = Cache.ImportedTextures.Find(WraithImage.ImagePtr))
					{
						if (Found->IsValid())
						{
							TextureAsset = Found->Get();
						}
						else
						{
							Cache.ImportedTextures.Remove(WraithImage.ImagePtr); // Remove stale
						}
					}
				}

				// --- Import if not cached ---
				if (!TextureAsset)
				{
					FString ImageName = FCoDAssetNameHelper::NoIllegalSigns(WraithImage.ImageName);
					if (!Handler->ReadImageDataToTexture(WraithImage.ImagePtr, TextureAsset, ImageName,
					                                     TexturePackagePath))
					{
						UE_LOG(LogITUAssetImporter, Warning,
						       TEXT("Failed to read/create texture %s (Ptr: 0x%llX) for material %s"),
						       *WraithImage.ImageName, WraithImage.ImagePtr, *WraithMaterial.MaterialName);
						continue;
					}
					if (TextureAsset)
					{
						// Add to cache
						Cache.AddCreatedAsset(TextureAsset);
						FScopeLock Lock(&Cache.CacheMutex);
						Cache.ImportedTextures.Add(WraithImage.ImagePtr, TextureAsset);
					}
					else
					{
						UE_LOG(LogITUAssetImporter, Warning,
						       TEXT("ReadImageDataToTexture returned null for %s (Ptr: 0x%llX)"),
						       *WraithImage.ImageName, WraithImage.ImagePtr);
						continue;
					}
				}
				WraithImage.ImageObject = TextureAsset;
			}


			// 2. Build FCastMaterialInfo (even if some textures failed)
			uint32 GlobalMaterialIndex = OutSceneRoot.Materials.Num();
			FCastMaterialInfo& CastMaterialInfo = OutSceneRoot.Materials.AddDefaulted_GetRef();

			FString MatName = FCoDAssetNameHelper::NoIllegalSigns(WraithMaterial.MaterialName);
			if (MatName.IsEmpty()) MatName = FString::Printf(TEXT("Mat_Hash_%llx"), WraithMaterial.MaterialHash);
			CastMaterialInfo.Name = MatName;
			CastMaterialInfo.MaterialHash = WraithMaterial.MaterialHash;
			CastMaterialInfo.MaterialPtr = WraithMaterial.MaterialPtr;

			CastMaterialInfo.Textures.Reserve(WraithMaterial.Images.Num());
			for (const FWraithXImage& WraithImage : WraithMaterial.Images)
			{
				if (!WraithImage.ImageObject) continue;

				FCastTextureInfo& CastTextureInfo = CastMaterialInfo.Textures.AddDefaulted_GetRef();
				CastTextureInfo.TextureName = WraithImage.ImageName;
				CastTextureInfo.TextureObject = WraithImage.ImageObject;
				FString ParameterName = FString::Printf(TEXT("Tex_Semantic_0x%X"), WraithImage.SemanticHash);
				CastTextureInfo.TextureSlot = ParameterName;
				CastTextureInfo.TextureType = ParameterName;
			}

			GlobalMaterialMap.Add(WraithMaterial.MaterialHash, GlobalMaterialIndex);

			// 3. Optionally Create UMaterialInstance Now
			if (bCreateMaterialInstances)
			{
				FString MaterialPackagePath = FPaths::Combine(Context.BaseImportPath, TEXT("Models"),
				                                              InOutModelData.ModelName, TEXT("Materials"));
				GetOrCreateMaterialInstance(CastMaterialInfo, Context, MaterialImporter, MaterialPackagePath);
			}
		}
	}

	OutSceneRoot.MaterialMap = GlobalMaterialMap;

	UE_LOG(LogITUAssetImporter, Log, TEXT("Processed dependencies for model %s. Prepared %d unique materials."),
	       *InOutModelData.ModelName, GlobalMaterialMap.Num());
	ProgressDelegate.ExecuteIfBound(BaseProgress + ProgressRange,
	                                NSLOCTEXT("ModelImporter", "DepsDone", "Dependencies Processed"));
	return true;
}

UMaterialInterface* FModelImporter::GetOrCreateMaterialInstance(const FCastMaterialInfo& PreparedMaterialInfo,
                                                                const FAssetImportContext& Context,
                                                                ICastMaterialImporter* MaterialImporter,
                                                                const FString& PreferredPackagePath)
{
	if (PreparedMaterialInfo.MaterialHash == 0) return nullptr;

	FImportedAssetsCache& Cache = *Context.AssetCache;

	// --- Check Cache ---
	{
		FScopeLock Lock(&Cache.CacheMutex);
		if (const TWeakObjectPtr<UMaterialInterface>* Found = Cache.ImportedMaterials.Find(PreparedMaterialInfo.MaterialHash))
		{
			if (Found->IsValid()) { return Found->Get(); }
			Cache.ImportedMaterials.Remove(PreparedMaterialInfo.MaterialHash);
		}
	}

	// --- Create New Instance ---
	FString MaterialAssetName = FCoDAssetNameHelper::NoIllegalSigns(PreparedMaterialInfo.Name);
	if (MaterialAssetName.IsEmpty())
		MaterialAssetName = FString::Printf(
			TEXT("MI_Hash_%llu"), PreparedMaterialInfo.MaterialHash);

	FString FullMaterialPackageName = FPaths::Combine(PreferredPackagePath, MaterialAssetName);
	UPackage* MaterialPackage = CreatePackage(*FullMaterialPackageName);
	if (!MaterialPackage)
	{
		UE_LOG(LogITUAssetImporter, Error,
		       TEXT("GetOrCreateMaterialInstance: Failed to create package for material: %s"),
		       *FullMaterialPackageName);
		return nullptr;
	}
	MaterialPackage->FullyLoad();

	FCastImportOptions MatOptions;
	UMaterialInterface* CreatedMaterial = MaterialImporter->CreateMaterialInstance(
		PreparedMaterialInfo,
		MatOptions,
		MaterialPackage
	);

	if (CreatedMaterial)
	{
		UE_LOG(LogITUAssetImporter, Verbose, TEXT("GetOrCreateMaterialInstance: Created material '%s' (Hash %llu)"),
		       *CreatedMaterial->GetName(), PreparedMaterialInfo.MaterialHash);
		FScopeLock Lock(&Cache.CacheMutex);
		Cache.ImportedMaterials.Add(PreparedMaterialInfo.MaterialHash, CreatedMaterial);
		Cache.AddCreatedAsset(CreatedMaterial);
	}
	else
	{
		UE_LOG(LogITUAssetImporter, Error,
		       TEXT("GetOrCreateMaterialInstance: CreateMaterialInstance failed for '%s' (Hash %llu)"),
		       *PreparedMaterialInfo.Name, PreparedMaterialInfo.MaterialHash);
	}

	return CreatedMaterial;
}
