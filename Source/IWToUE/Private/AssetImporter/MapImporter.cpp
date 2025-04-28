#include "AssetImporter/MapImporter.h"

#include "FileHelpers.h"
#include "Animation/SkeletalMeshActor.h"
#include "AssetImporter/ModelImporter.h"
#include "CastManager/CastImportOptions.h"
#include "CastManager/CastRoot.h"
#include "CastManager/DefaultCastMaterialImporter.h"
#include "CastManager/DefaultCastMeshImporter.h"
#include "Database/CoDDatabaseService.h"
#include "Engine/StaticMeshActor.h"
#include "Interface/IGameAssetHandler.h"
#include "UObject/SavePackage.h"
#include "Utils/CoDAssetHelper.h"
#include "WraithX/CoDAssetType.h"

bool FMapImporter::Import(const FString& ImportPath, TSharedPtr<FCoDAsset> Asset, IGameAssetHandler* Handler,
                          FAssetImportManager* Manager)
{
	TSharedPtr<FCoDMap> MapInfo = StaticCastSharedPtr<FCoDMap>(Asset);
	if (!MapInfo.IsValid() || !Handler || !Manager)
	{
		UE_LOG(LogTemp, Error, TEXT("Import failed: Invalid input parameters."));
		return false;
	}
	GWarn->BeginSlowTask(
		FText::Format(
			NSLOCTEXT("MapImporter", "ImportingMap", "Importing Map: {0}"), FText::FromString(MapInfo->AssetName)),
		true);

	FWraithXMap MapData;
	MapData.MapName = FPaths::GetBaseFilename(MapInfo->AssetName);
	if (!Handler->ReadMapData(MapInfo, MapData))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to read map data for %s."), *MapInfo->AssetName);
		GWarn->EndSlowTask();
		return false;
	}
	FString BasePackagePath = ImportPath; // e.g., /Game/Imports/MyMap
	FString MapMeshesPackagePath = FPaths::Combine(BasePackagePath, TEXT("Meshes"));
	FString MapMaterialsPackagePath = FPaths::Combine(BasePackagePath, TEXT("Materials"));
	FString MapTexturesPackagePath = FPaths::Combine(MapMaterialsPackagePath, TEXT("Textures"));
	FString StaticModelsPackagePath = FPaths::Combine(BasePackagePath, TEXT("Models"));
	FString ModelMaterialsPackagePath = FPaths::Combine(StaticModelsPackagePath, TEXT("Materials"));
	FString ModelTexturesPackagePath = FPaths::Combine(ModelMaterialsPackagePath, TEXT("Textures"));

	TSharedPtr<ICastMaterialImporter> MaterialImporter = MakeShared<FDefaultCastMaterialImporter>();
	TUniquePtr<ICastMeshImporter> MeshImporter = MakeUnique<FDefaultCastMeshImporter>();
	FImportedAssetsCache ImportCache;
	FCastImportOptions ImportOptions;

	// 预处理材质和贴图
	GWarn->StatusUpdate(0, 3, NSLOCTEXT("MapImporter", "Preprocessing",
	                                    "Phase 1/3: Pre-processing Materials & Textures..."));

	TMap<uint64, FCastMaterialInfo> MapMeshMaterialInfos;
	UE_LOG(LogTemp, Log, TEXT("Processing materials for %d map mesh chunks..."), MapData.MapMeshes.Num());
	for (FWraithMapMeshData& MeshChunkData : MapData.MapMeshes)
	{
		if (GWarn->ReceivedUserCancel()) return false;
		if (MeshChunkData.MeshData.MaterialPtr == 0 && MeshChunkData.MeshData.MaterialHash == 0) continue;
		if (MapMeshMaterialInfos.Contains(MeshChunkData.MeshData.MaterialHash)) continue;

		FWraithXMaterial MaterialData;
		uint64 CurrentMaterialHash = MeshChunkData.MeshData.MaterialHash;
		if (MeshChunkData.MeshData.MaterialPtr != 0)
		{
			if (!Handler->ReadMaterialDataFromPtr(MeshChunkData.MeshData.MaterialPtr, MaterialData))
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to read material data from Ptr 0x%llX for map mesh chunk '%s'."),
				       MeshChunkData.MeshData.MaterialPtr, *MeshChunkData.MeshName);
				continue;
			}
			// Verify or update hash
			if (CurrentMaterialHash != 0 && MaterialData.MaterialHash != CurrentMaterialHash)
			{
				UE_LOG(LogTemp, Warning,
				       TEXT(
					       "Material hash mismatch for map mesh chunk '%s'. Expected %llu, Read %llu. Using read hash %llu."
				       ),
				       *MeshChunkData.MeshName, CurrentMaterialHash, MaterialData.MaterialHash,
				       MaterialData.MaterialHash);
			}
			CurrentMaterialHash = MaterialData.MaterialHash; // Use the hash read from data
			if (MapMeshMaterialInfos.Contains(CurrentMaterialHash)) continue;
			// Check again with potentially updated hash
		}
		else if (CurrentMaterialHash != 0)
		{
			// Cannot read from Ptr, need to rely solely on hash - MaterialData will be minimal
			MaterialData.MaterialHash = CurrentMaterialHash;
			MaterialData.MaterialName = FString::Printf(
				TEXT("xmaterial_%llx"), (CurrentMaterialHash & 0xFFFFFFFFFFFFFFF));
			// Textures might be missing here if not read from pointer!
			UE_LOG(LogTemp, Warning,
			       TEXT("No MaterialPtr for map mesh chunk '%s', MaterialHash %llu. Material info might be incomplete."
			       ), *MeshChunkData.MeshName, CurrentMaterialHash);
		}
		else
		{
			continue;
		}

		FCastMaterialInfo PreparedInfo;
		if (PrepareMaterialInfo(MeshChunkData.MeshData.MaterialPtr, MaterialData, PreparedInfo, Handler,
		                        MapTexturesPackagePath, ImportCache))
		{
			MapMeshMaterialInfos.Add(CurrentMaterialHash, PreparedInfo);
		}
	}

	// 导入地图网格Chunks
	int32 TotalImportTasks = MapData.MapMeshes.Num();
	TSet<uint64> UniqueStaticModelPtrSet;
	for (const FWraithMapStaticModelInstance& Instance : MapData.StaticModelInstances)
	{
		UniqueStaticModelPtrSet.Add(Instance.ModelAssetPtr);
	}
	TotalImportTasks += UniqueStaticModelPtrSet.Num();
	int32 CompletedTasks = 0;
	GWarn->StatusUpdate(1, 3, FText::Format(
		                    NSLOCTEXT("MapImporter", "ImportingMapMeshes",
		                              "Phase 2/3: Importing {0} Map Mesh Chunks..."),
		                    FText::AsNumber(MapData.MapMeshes.Num())));

	TArray<TWeakObjectPtr<UStaticMesh>> ImportedMapMeshes;
	ImportedMapMeshes.SetNum(MapData.MapMeshes.Num());

	for (int32 Index = 0; Index < MapData.MapMeshes.Num(); ++Index)
	{
		if (GWarn->ReceivedUserCancel()) return false;
		GWarn->UpdateProgress(CompletedTasks, TotalImportTasks);
		const FWraithMapMeshData& MeshChunkData = MapData.MapMeshes[Index];
	
		// Find the pre-processed material info for this chunk
		const FCastMaterialInfo* FoundMaterialInfo = MapMeshMaterialInfos.Find(MeshChunkData.MeshData.MaterialHash);
	
		// Handle potential hash mismatch case again if needed
		if (!FoundMaterialInfo && MeshChunkData.MeshData.MaterialPtr != 0)
		{
			FWraithXMaterial TempMatData;
			if (Handler->ReadMaterialDataFromPtr(MeshChunkData.MeshData.MaterialPtr, TempMatData))
			{
				FoundMaterialInfo = MapMeshMaterialInfos.Find(TempMatData.MaterialHash);
			}
		}
	
		if (!FoundMaterialInfo)
		{
			UE_LOG(LogTemp, Error,
			       TEXT(
				       "Could not find pre-processed material info for map mesh chunk '%s' (Hash %llu). Skipping mesh import."
			       ), *MeshChunkData.MeshName, MeshChunkData.MeshData.MaterialHash);
			CompletedTasks++;
			continue;
		}
	
		// Get or Create the Material Asset *before* importing the mesh
		UMaterialInterface* MaterialAsset = GetOrCreateMaterialAsset(
			*FoundMaterialInfo,
			MaterialImporter.Get(),
			MapMaterialsPackagePath, // Materials for map meshes go here
			ImportOptions,
			ImportCache
		);
	
		if (!MaterialAsset)
		{
			UE_LOG(LogTemp, Error,
			       TEXT(
				       "Failed to get or create material asset for map mesh chunk '%s' (Hash %llu). Skipping mesh import."
			       ), *MeshChunkData.MeshName, MeshChunkData.MeshData.MaterialHash);
			CompletedTasks++;
			continue;
		}
	
		// Now import the mesh chunk
		UStaticMesh* ImportedMesh = ImportMapMeshChunk(
			MeshChunkData,
			*FoundMaterialInfo, // Pass the prepared info (for scene setup)
			MapMeshesPackagePath, // Path for the mesh asset
			MeshImporter.Get(), // Pass mesh importer instance
			MaterialImporter.Get(), // Pass material importer instance
			ImportCache // Pass the main cache
		);
	
		if (ImportedMesh)
		{
			ImportedMapMeshes[Index] = ImportedMesh; // Store weak ptr
			// Explicitly assign the material again AFTER import for robustness,
			// as the dummy mesh importer might not do it correctly.
			if (ImportedMesh->GetStaticMaterials().Num() > 0)
			{
				if (ImportedMesh->GetStaticMaterials()[0].MaterialInterface != MaterialAsset)
				{
					UE_LOG(LogTemp, Warning, TEXT("Re-assigning material '%s' to mesh '%s' slot 0 after import."),
					       *MaterialAsset->GetName(), *ImportedMesh->GetName());
					ImportedMesh->SetMaterial(0, MaterialAsset);
					ImportCache.AddCreatedAsset(ImportedMesh); // Mark dirty again
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning,
				       TEXT("Mesh '%s' has no material slots after import. Cannot assign material '%s'."),
				       *ImportedMesh->GetName(), *MaterialAsset->GetName());
			}
		}
		CompletedTasks++;
	}

	// 导入静态网格
	GWarn->StatusUpdate(2, 3, FText::Format(
		                    NSLOCTEXT("MapImporter", "ImportingStaticModels",
		                              "Phase 3/3: Importing {0} Static Models..."),
		                    FText::AsNumber(UniqueStaticModelPtrSet.Num())));
	TArray<uint64> UniquePtrArray = UniqueStaticModelPtrSet.Array();
	for (int32 Index = 0; Index < UniquePtrArray.Num(); ++Index)
	{
		if (GWarn->ReceivedUserCancel()) return false;
		GWarn->UpdateProgress(CompletedTasks, TotalImportTasks);
		uint64 ModelPtr = UniquePtrArray[Index];
	
		// ImportStaticModelAsset now handles its own material prep/creation using the cache
		UObject* ImportedModel = ImportStaticModelAsset(
			ModelPtr,
			StaticModelsPackagePath, // Base path for models
			Handler,
			MaterialImporter.Get(), // Pass the same importer instances
			MeshImporter.Get(),
			ImportCache // Pass the main cache
		);
		CompletedTasks++;
	}

	// --- Final Steps ---
	if (GWarn->ReceivedUserCancel()) return false;
	GWarn->StatusUpdate(3, 3, NSLOCTEXT("MapImporter", "SavingAssets", "Saving Assets..."));
	SaveImportedAssets(ImportCache);

	if (GWarn->ReceivedUserCancel()) return false;
	GWarn->StatusUpdate(3, 3, NSLOCTEXT("MapImporter", "PlacingActors", "Placing Actors in Level..."));
	PlaceActorsInLevel(MapData, ImportedMapMeshes, ImportCache);

	GWarn->EndSlowTask();
	UE_LOG(LogTemp, Log, TEXT("Map import completed for: %s"), *MapInfo->AssetName);
	return true;
}

UStaticMesh* FMapImporter::ImportMapMeshChunk(const FWraithMapMeshData& MapMeshData,
                                              const FCastMaterialInfo& MaterialInfo, const FString& BaseImportPath,
                                              ICastMeshImporter* MeshImporter,
                                              ICastMaterialImporter* MaterialImporter, FImportedAssetsCache& Cache)
{
	if (MapMeshData.MeshData.VertexPositions.IsEmpty() || MapMeshData.MeshData.Faces.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Skipping map mesh chunk '%s' due to empty geometry."), *MapMeshData.MeshName);
		return nullptr;
	}

	FCastScene Scene;
	FCastRoot& Root = Scene.Roots.AddDefaulted_GetRef();

	Root.Materials.Add(MaterialInfo);
	Root.MaterialMap.Add(MaterialInfo.MaterialHash, 0); // Map hash to index 0 in this scene's list

	FCastMeshInfo MeshDataCopy = MapMeshData.MeshData;
	MeshDataCopy.MaterialIndex = 0; // Point to the material at index 0 in Root.Materials
	MeshDataCopy.Name = FCoDAssetNameHelper::NoIllegalSigns(MapMeshData.MeshName); // Ensure mesh name is clean
	if (MeshDataCopy.Name.IsEmpty())
		MeshDataCopy.Name = FString::Printf(
			TEXT("MapMeshChunk_Mat_%llx"), MaterialInfo.MaterialHash);

	Root.Models.AddDefaulted();
	Root.Models[0].Meshes.Add(MoveTemp(MeshDataCopy));
	Root.Models[0].Name = MeshDataCopy.Name; // Assign name to model info as well


	// --- Create Package and Import ---
	FString AssetName = MeshDataCopy.Name; // Use the cleaned name
	FString FullPackageName = FPaths::Combine(BaseImportPath, AssetName);
	UE_LOG(LogTemp, Verbose, TEXT("ImportMapMeshChunk: Attempting to create package: %s"), *FullPackageName);

	UPackage* Package = CreatePackage(*FullPackageName);
	if (!Package)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create package: %s"), *FullPackageName);
		return nullptr;
	}
	Package->FullyLoad(); // Required before modifying?

	FCastImportOptions Options;
	TArray<UObject*> CreatedAssetsForChunk;

	UStaticMesh* ImportedMesh = MeshImporter->ImportStaticMesh(
		Scene, Options, Package, *AssetName, RF_Public | RF_Standalone, MaterialImporter, TEXT(""),
		CreatedAssetsForChunk);

	if (ImportedMesh)
	{
		UE_LOG(LogTemp, Verbose, TEXT("Successfully imported map mesh chunk: %s"), *AssetName);
		// Add the primary mesh asset AND any other assets created during its import
		Cache.AddCreatedAsset(ImportedMesh);
		Cache.AddCreatedAssets(CreatedAssetsForChunk); // Ensure all dependencies are marked for saving

		if (!Cache.PackagesToSave.Contains(Package))
		{
			UE_LOG(LogTemp, Warning,
			       TEXT(
				       "ImportMapMeshChunk: Package '%s' was not marked for saving after importing mesh '%s'. Marking manually."
			       ), *Package->GetName(), *AssetName);
			Cache.PackagesToSave.Add(Package);
			Package->MarkPackageDirty();
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to import map mesh chunk: %s"), *MapMeshData.MeshName);
	}

	return ImportedMesh;
}

UObject* FMapImporter::ImportStaticModelAsset(
	uint64 ModelPtr, const FString& BaseModelsPackagePath,
	IGameAssetHandler* Handler, ICastMaterialImporter* MaterialImporter,
	ICastMeshImporter* MeshImporter,
	FImportedAssetsCache& Cache)
{
	// --- Check Cache ---
	{
		FScopeLock Lock(&AssetMapMutex);
		if (TWeakObjectPtr<UObject>* Found = Cache.ImportedStaticModels.Find(ModelPtr))
		{
			if (Found->IsValid())
			{
				UE_LOG(LogTemp, Verbose, TEXT("Static model asset with Ptr 0x%llX already imported and cached."),
				       ModelPtr);
				return Found->Get();
			}
			Cache.ImportedStaticModels.Remove(ModelPtr); // Remove stale entry
		}
	}

	// --- Read Model Data ---
	TSharedPtr<FCoDModel> ModelAssetInfo = MakeShared<FCoDModel>();
	ModelAssetInfo->AssetPointer = ModelPtr;
	ModelAssetInfo->AssetName.Empty();

	FWraithXModel GenericModel;
	if (!Handler->ReadModelData(ModelAssetInfo, GenericModel)) // ReadModelData should fill GenericModel.ModelName
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to read model data for Ptr 0x%llX (Name: %s)."), ModelPtr,
		       *ModelAssetInfo->AssetName);
		return nullptr;
	}
	ModelAssetInfo->AssetName = FCoDAssetNameHelper::NoIllegalSigns(GenericModel.ModelName);
	if (ModelAssetInfo->AssetName.IsEmpty())
	{
		ModelAssetInfo->AssetName = FString::Printf(TEXT("xmodel_ptr_%llx"), ModelPtr); // Fallback name
		GenericModel.ModelName = ModelAssetInfo->AssetName; // Update GenericModel too
	}

	FString AssetBaseName = FPaths::GetBaseFilename(ModelAssetInfo->AssetName);
	FString ModelPackagePath = FPaths::Combine(BaseModelsPackagePath, AssetBaseName);
	FString ModelMaterialsPath = FPaths::Combine(ModelPackagePath, TEXT("Materials"));
	FString ModelTexturesPath = FPaths::Combine(ModelMaterialsPath, TEXT("Textures"));

	TMap<uint64, UTexture2D*> ImportedTextures;
	for (FWraithXModelLod& LodModel : GenericModel.ModelLods)
	{
		for (FWraithXMaterial& Material : LodModel.Materials)
		{
			for (FWraithXImage& Image : Material.Images)
			{
				if (Image.ImagePtr == 0 || ImportedTextures.Contains(Image.ImagePtr))
				{
					Image.ImageObject = ImportedTextures[Image.ImagePtr];
					continue;
				}
				UTexture2D* TextureObj = nullptr;
				Image.ImageName = FCoDAssetNameHelper::NoIllegalSigns(Image.ImageName);
				if (Handler->ReadImageDataToTexture(Image.ImagePtr, TextureObj, Image.ImageName, ModelTexturesPath))
				{
					if (TextureObj)
					{
						Image.ImageObject = TextureObj;
						ImportedTextures.Add(Image.ImagePtr, TextureObj);
					}
					else
					{
						UE_LOG(LogTemp, Warning,
							   TEXT("Handler::ReadImageDataToTexture succeeded but returned null texture for %s"),
							   *Image.ImageName);
					}
				}
			}
		}
	}
	FCastRoot SceneRoot;
	int32 EstimatedMaterialCount = 0;
	for (const FWraithXModelLod& LodModel : GenericModel.ModelLods)
	{
		EstimatedMaterialCount += LodModel.Materials.Num();
	}
	SceneRoot.Materials.Reserve(EstimatedMaterialCount);

	for (FWraithXModelLod& LodModel : GenericModel.ModelLods)
	{
		for (FWraithXMaterial& Material : LodModel.Materials)
		{
			if (!SceneRoot.MaterialMap.Contains(Material.MaterialHash))
			{
				uint32 MaterialGlobalIndex = SceneRoot.Materials.Num();
				FCastMaterialInfo& MaterialInfo = SceneRoot.Materials.AddDefaulted_GetRef();

				FString MaterialName = FCoDAssetNameHelper::NoIllegalSigns(Material.MaterialName);
				if (MaterialName.IsEmpty())
				{
					MaterialName = FString::Printf(TEXT("xmaterial_%llx"), (Material.MaterialHash & 0xFFFFFFFFFFFFFFF));
				}
				MaterialInfo.Name = MaterialName;
				MaterialInfo.MaterialHash = Material.MaterialHash;

				MaterialInfo.Textures.Reserve(Material.Images.Num());
				for (const FWraithXImage& Image : Material.Images)
				{
					if (!Image.ImageObject) continue;

					FCastTextureInfo& TextureInfo = MaterialInfo.Textures.AddDefaulted_GetRef();

					TextureInfo.TextureName = Image.ImageName;
					TextureInfo.TextureObject = Image.ImageObject;
					TextureInfo.TextureSlot = FString::Printf(TEXT("unk_semantic_0x%x"), Image.SemanticHash);
					TextureInfo.TextureType = TextureInfo.TextureSlot;
				}

				SceneRoot.MaterialMap.Add(Material.MaterialHash, MaterialGlobalIndex);
			}
		}
	}

	bool bHasGeometry = false;
    for (int32 LodIdx = 0; LodIdx < GenericModel.ModelLods.Num(); ++LodIdx) {
        FCastModelInfo ModelResult;
        if (Handler->TranslateModel(GenericModel, LodIdx, ModelResult, SceneRoot)) {
            if (!ModelResult.Meshes.IsEmpty()) {
                 bHasGeometry = true;
                 FCastModelLod ModelLod;
                 ModelLod.Distance = GenericModel.ModelLods[LodIdx].LodDistance;
                 ModelLod.MaxDistance = GenericModel.ModelLods[LodIdx].LodMaxDistance;
                 ModelLod.ModelIndex = SceneRoot.Models.Add(MoveTemp(ModelResult));
                 SceneRoot.ModelLodInfo.Add(ModelLod);
            }
        } else {
            UE_LOG(LogTemp, Warning, TEXT("ImportStaticModelAsset: Failed to translate LOD %d for model %s."), LodIdx, *AssetBaseName);
        }
    }

    if (!bHasGeometry) {
        UE_LOG(LogTemp, Error, TEXT("ImportStaticModelAsset: No valid geometry translated for model %s. Import failed."), *AssetBaseName);
        return nullptr;
    }

    FString FullPackageName = FPaths::Combine(ModelPackagePath, AssetBaseName);
    UPackage* Package = CreatePackage(*FullPackageName);
    if (!Package) {
        UE_LOG(LogTemp, Error, TEXT("ImportStaticModelAsset: Failed to create package: %s"), *FullPackageName);
        return nullptr;
    }
    Package->FullyLoad();

	FCastImportOptions Options;
    TArray<UObject*> OutCreatedObjects;
    UObject* ImportedMeshObject = nullptr;
    bool bIsSkeletal = !SceneRoot.Models.IsEmpty() && !SceneRoot.Models[0].Skeletons.IsEmpty() && !SceneRoot.Models[0].Skeletons[0].Bones.IsEmpty();

	FCastScene Scene;
	Scene.Roots.Add(SceneRoot);
    ImportedMeshObject = MeshImporter->ImportStaticMesh(
        Scene, Options, Package, *AssetBaseName, RF_Public | RF_Standalone,
        MaterialImporter, TEXT(""), OutCreatedObjects);

    if (ImportedMeshObject) {
        UE_LOG(LogTemp, Log, TEXT("ImportStaticModelAsset: Successfully imported model asset '%s' into package '%s'."), *ImportedMeshObject->GetName(), *Package->GetName());
        Cache.AddCreatedAsset(ImportedMeshObject);
        Cache.AddCreatedAssets(OutCreatedObjects);
        {
            FScopeLock Lock(&AssetMapMutex);
            Cache.ImportedStaticModels.Add(ModelPtr, ImportedMeshObject);
        }
        return ImportedMeshObject;
    }
    UE_LOG(LogTemp, Error, TEXT("ImportStaticModelAsset: Mesh Importer failed for model: %s"), *AssetBaseName);
    return nullptr;
}

void FMapImporter::PlaceActorsInLevel(const FWraithXMap& MapData,
                                      const TArray<TWeakObjectPtr<UStaticMesh>>& ImportedMapMeshes,
                                      FImportedAssetsCache& Cache)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("Cannot place actors: Editor world context is invalid."));
		return;
	}
	ULevel* CurrentLevel = World->GetCurrentLevel();
	if (!CurrentLevel)
	{
		UE_LOG(LogTemp, Error, TEXT("Cannot place actors: Current level is invalid."));
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.OverrideLevel = CurrentLevel;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// --- Place Map Meshes ---
	UE_LOG(LogTemp, Log, TEXT("Placing %d map mesh actors..."), ImportedMapMeshes.Num());
	int32 InstanceCounter = 0;
	for (int32 i = 0; i < ImportedMapMeshes.Num(); ++i)
	{
		if (!ImportedMapMeshes[i].IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("Skipping placement for null or pending kill map mesh asset at index %d."),
			       i);
			continue;
		}
		UStaticMesh* MeshAsset = ImportedMapMeshes[i].Get();

		FString ActorName = FString::Printf(TEXT("%s_Chunk_%d"), *MapData.MapName, i); // More descriptive name
		SpawnParams.Name = MakeUniqueObjectName(CurrentLevel, AStaticMeshActor::StaticClass(), FName(*ActorName));

		AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>(
			AStaticMeshActor::StaticClass(), FTransform::Identity, SpawnParams);

		if (MeshActor)
		{
			MeshActor->SetMobility(EComponentMobility::Static);
			UStaticMeshComponent* MeshComponent = MeshActor->GetStaticMeshComponent();
			if (MeshComponent)
			{
				MeshComponent->SetStaticMesh(MeshAsset);
				// Material verification/assignment happened in ImportMapMeshChunk
				MeshComponent->SetRelativeTransform(FTransform::Identity); // Redundant? Spawn sets world transform.
				UE_LOG(LogTemp, Verbose, TEXT("Placed map mesh actor: %s using mesh %s"), *MeshActor->GetName(),
				       *MeshAsset->GetName());
			}
			MeshActor->SetActorLabel(ActorName); // Set user-facing label
			MeshActor->RerunConstructionScripts();
			MeshActor->MarkPackageDirty(); // Mark actor's level dirty
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to spawn AStaticMeshActor for map mesh: %s"), *MeshAsset->GetName());
		}
	}

	// --- Place Static Model Instances ---
	UE_LOG(LogTemp, Log, TEXT("Placing %d static model instance actors..."), MapData.StaticModelInstances.Num());
	InstanceCounter = 0;
	for (const FWraithMapStaticModelInstance& InstanceInfo : MapData.StaticModelInstances)
	{
		TWeakObjectPtr<UObject>* FoundAssetPtr = nullptr;
		{
			FScopeLock Lock(&AssetMapMutex); // Protect cache access
			FoundAssetPtr = Cache.ImportedStaticModels.Find(InstanceInfo.ModelAssetPtr);
		}


		if (!FoundAssetPtr || !FoundAssetPtr->IsValid())
		{
			UE_LOG(LogTemp, Warning,
			       TEXT("Skipping placement for instance '%s': Could not find or load imported asset for hash %llu."),
			       *InstanceInfo.InstanceName, InstanceInfo.ModelAssetPtr);
			continue;
		}

		UObject* ModelAsset = FoundAssetPtr->Get();
		AActor* PlacedActor = nullptr;
		FString BaseActorName = FCoDAssetNameHelper::NoIllegalSigns(InstanceInfo.InstanceName);
		if (BaseActorName.IsEmpty()) BaseActorName = FPaths::GetBaseFilename(ModelAsset->GetName());
		FString ActorName = FString::Printf(TEXT("%s_Inst_%d"), *BaseActorName, InstanceCounter++);

		if (UStaticMesh* StaticMeshAsset = Cast<UStaticMesh>(ModelAsset))
		{
			SpawnParams.Name = MakeUniqueObjectName(CurrentLevel, AStaticMeshActor::StaticClass(), FName(*ActorName));
			AStaticMeshActor* StaticMeshActor = World->SpawnActor<AStaticMeshActor>(
				AStaticMeshActor::StaticClass(), InstanceInfo.Transform, SpawnParams);
			if (StaticMeshActor)
			{
				StaticMeshActor->SetMobility(EComponentMobility::Static); // Or Movable based on game data?
				UStaticMeshComponent* MeshComponent = StaticMeshActor->GetStaticMeshComponent();
				if (MeshComponent)
				{
					MeshComponent->SetStaticMesh(StaticMeshAsset);
					// Materials should be correctly assigned on the StaticMeshAsset already
					UE_LOG(LogTemp, Verbose, TEXT("Placed static mesh actor: %s using mesh %s"),
					       *StaticMeshActor->GetName(), *StaticMeshAsset->GetName());
				}
				StaticMeshActor->SetActorLabel(ActorName);
				StaticMeshActor->RerunConstructionScripts();
				StaticMeshActor->MarkPackageDirty(); // Mark actor's level dirty
				PlacedActor = StaticMeshActor;
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to spawn AStaticMeshActor for instance: %s"),
				       *InstanceInfo.InstanceName);
			}
		}
		else if (USkeletalMesh* SkeletalMeshAsset = Cast<USkeletalMesh>(ModelAsset))
		{
			SpawnParams.Name = MakeUniqueObjectName(CurrentLevel, ASkeletalMeshActor::StaticClass(), FName(*ActorName));
			ASkeletalMeshActor* SkeletalMeshActor = World->SpawnActor<ASkeletalMeshActor>(
				ASkeletalMeshActor::StaticClass(), InstanceInfo.Transform, SpawnParams);
			if (SkeletalMeshActor)
			{
				// Skeletal meshes usually Movable by default
				USkeletalMeshComponent* MeshComponent = SkeletalMeshActor->GetSkeletalMeshComponent();
				if (MeshComponent)
				{
					MeshComponent->SetSkeletalMesh(SkeletalMeshAsset);
					// Materials should be correctly assigned on the SkeletalMeshAsset already
					UE_LOG(LogTemp, Verbose, TEXT("Placed skeletal mesh actor: %s using mesh %s"),
					       *SkeletalMeshActor->GetName(), *SkeletalMeshAsset->GetName());
				}
				SkeletalMeshActor->SetActorLabel(ActorName);
				SkeletalMeshActor->RerunConstructionScripts();
				SkeletalMeshActor->MarkPackageDirty(); // Mark actor's level dirty
				PlacedActor = SkeletalMeshActor;
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to spawn ASkeletalMeshActor for instance: %s"),
				       *InstanceInfo.InstanceName);
			}
		}
		else
		{
			UE_LOG(LogTemp, Error,
			       TEXT("Imported asset for Ptr %llx (Name: %s) is neither Static nor Skeletal Mesh: %s"),
			       InstanceInfo.ModelAssetPtr, *InstanceInfo.InstanceName, *ModelAsset->GetPathName());
		}
	}

	GEditor->RedrawLevelEditingViewports(true);
	UE_LOG(LogTemp, Log, TEXT("Finished placing actors."));
}

bool FMapImporter::PrepareMaterialInfo(uint64 MaterialPtr, FWraithXMaterial& InMaterialData,
                                       FCastMaterialInfo& OutMaterialInfo, IGameAssetHandler* Handler,
                                       const FString& BaseTexturePath, FImportedAssetsCache& Cache)
{
	FString MaterialName = FCoDAssetNameHelper::NoIllegalSigns(InMaterialData.MaterialName);
	OutMaterialInfo.Name = MaterialName;
	OutMaterialInfo.MaterialHash = InMaterialData.MaterialHash;
	OutMaterialInfo.Textures.Reserve(InMaterialData.Images.Num());

	bool bSuccess = true;
	for (FWraithXImage& ImageInfo : InMaterialData.Images)
	{
		UTexture2D* TextureObj = ProcessTexture(ImageInfo.ImagePtr, ImageInfo.ImageName, Handler, BaseTexturePath,
		                                        Cache);

		if (TextureObj)
		{
			FCastTextureInfo& TextureInfo = OutMaterialInfo.Textures.AddDefaulted_GetRef();
			TextureInfo.TextureName = ImageInfo.ImageName;
			TextureInfo.TextureObject = TextureObj;
			TextureInfo.TextureSlot = FString::Printf(TEXT("unk_semantic_0x%x"), ImageInfo.SemanticHash);
			TextureInfo.TextureType = TextureInfo.TextureSlot;
			ImageInfo.ImageObject = TextureObj;
		}
		else
		{
			UE_LOG(LogTemp, Warning,
			       TEXT(
				       "PrepareMaterialInfo: Failed to process texture '%s' (Ptr 0x%llX) for material '%s' (Hash %llu). Material may be incomplete."
			       ),
			       *ImageInfo.ImageName, ImageInfo.ImagePtr, *MaterialName, InMaterialData.MaterialHash);
		}
	}

	return bSuccess;
}

UTexture2D* FMapImporter::ProcessTexture(uint64 ImagePtr, FString& InOutImageName, IGameAssetHandler* Handler,
                                         const FString& BaseTexturePath, FImportedAssetsCache& Cache)
{
	if (ImagePtr == 0) return nullptr;

	{
		FScopeLock Lock(&AssetMapMutex);
		if (TWeakObjectPtr<UTexture2D>* Found = Cache.ImportedTextures.Find(ImagePtr))
		{
			if (Found->IsValid())
			{
				return Found->Get();
			}
			Cache.ImportedTextures.Remove(ImagePtr);
		}
	}

	InOutImageName = FCoDAssetNameHelper::NoIllegalSigns(InOutImageName);

	UTexture2D* TextureObj = nullptr;

	if (!FCoDAssetNameHelper::FindAssetByName(InOutImageName, TextureObj))
	{
		FString TexturePackagePath = FPaths::Combine(BaseTexturePath, InOutImageName);
		if (Handler->ReadImageDataToTexture(ImagePtr, TextureObj, InOutImageName, TexturePackagePath))
		{
			if (TextureObj)
			{
				Cache.AddCreatedAsset(TextureObj);
			}
			else
			{
				UE_LOG(LogTemp, Warning,
				       TEXT(
					       "ProcessTexture: Handler::ReadImageDataToTexture succeeded but returned null texture for Ptr 0x%llX, Name '%s'"
				       ), ImagePtr, *InOutImageName);
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ProcessTexture: Failed to read image data for Ptr 0x%llX, Name '%s'"),
			       ImagePtr, *InOutImageName);
			return nullptr;
		}
	}

	if (TextureObj)
	{
		FScopeLock Lock(&AssetMapMutex);
		Cache.ImportedTextures.Add(ImagePtr, TextureObj);
	}

	return TextureObj;
}

void FMapImporter::SaveImportedAssets(FImportedAssetsCache& Cache)
{
	if (Cache.PackagesToSave.IsEmpty())
	{
		UE_LOG(LogTemp, Log, TEXT("No new or modified packages to save."));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Saving %d packages..."), Cache.PackagesToSave.Num());

	TArray<UPackage*> PackagesArray = Cache.PackagesToSave.Array();

	// Consider using FEditorFileUtils::SavePackages instead for better integration
	// bool bSuccess = FEditorFileUtils::SavePackages(PackagesArray, true);
	// if (!bSuccess) {
	//     UE_LOG(LogTemp, Error, TEXT("Failed to save one or more packages using FEditorFileUtils::SavePackages."));
	// }

	// Manual saving loop (alternative)
	bool bPromptUser = false; // Set to true to ask user before saving
	if (bPromptUser)
	{
		if (FEditorFileUtils::PromptForCheckoutAndSave(PackagesArray, true, false) != FEditorFileUtils::PR_Success)
		{
			UE_LOG(LogTemp, Warning, TEXT("User cancelled saving operation."));
			return;
		}
		// If user accepted, SavePackages was already called by PromptForCheckoutAndSave
	}
	else
	{
		// Save directly without prompt
		for (UPackage* Pkg : PackagesArray)
		{
			if (Pkg && Pkg != GetTransientPackage() && Pkg->IsDirty()) // Double check it's valid and dirty
			{
				FString PackageFileName = FPackageName::LongPackageNameToFilename(
					Pkg->GetName(), FPackageName::GetAssetPackageExtension());
				UE_LOG(LogTemp, Verbose, TEXT("Saving package: %s to %s"), *Pkg->GetName(), *PackageFileName);

				FSavePackageArgs SaveArgs;
				SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
				SaveArgs.SaveFlags = SAVE_NoError; // Or SAVE_None? Add flags as needed.
				SaveArgs.bForceByteSwapping = false;
				SaveArgs.bWarnOfLongFilename = true;

				bool bSaved = UPackage::SavePackage(Pkg, nullptr, *PackageFileName, SaveArgs);

				if (!bSaved)
				{
					UE_LOG(LogTemp, Error, TEXT("Failed to save package: %s"), *Pkg->GetName());
				}
				else
				{
					// Optional: Add to source control after saving
					// FSourceControlModule::Get().GetProvider().Execute(ISourceControlOperation::Create<FMarkForAdd>(), Pkg);
				}
			}
		}
	}


	UE_LOG(LogTemp, Log, TEXT("Finished saving assets attempt."));
	Cache.PackagesToSave.Empty(); // Clear the set after attempting to save
}

UMaterialInterface* FMapImporter::GetOrCreateMaterialAsset(const FCastMaterialInfo& MaterialInfo,
                                                           ICastMaterialImporter* MaterialImporter,
                                                           const FString& PreferredPackagePath,
                                                           const FCastImportOptions& Options,
                                                           FImportedAssetsCache& Cache)
{
	if (MaterialInfo.MaterialHash == 0) return nullptr; // Cannot lookup without hash

	// Check cache first
	{
		FScopeLock Lock(&AssetMapMutex);
		if (TWeakObjectPtr<UMaterialInterface>* Found = Cache.ImportedMaterials.Find(MaterialInfo.MaterialHash))
		{
			if (Found->IsValid())
			{
				return Found->Get();
			}
			Cache.ImportedMaterials.Remove(MaterialInfo.MaterialHash); // Remove stale entry
		}
	}

	// Not cached or stale, create it
	FString MaterialAssetName = FCoDAssetNameHelper::NoIllegalSigns(MaterialInfo.Name);
	if (MaterialAssetName.IsEmpty())
		MaterialAssetName = FString::Printf(
			TEXT("MI_Hash_%llu"), MaterialInfo.MaterialHash);

	FString FullMaterialPackageName = FPaths::Combine(PreferredPackagePath, MaterialAssetName);
	UPackage* MaterialPackage = CreatePackage(*FullMaterialPackageName);
	if (!MaterialPackage)
	{
		UE_LOG(LogTemp, Error, TEXT("GetOrCreateMaterialAsset: Failed to create package for material: %s"),
		       *FullMaterialPackageName);
		return nullptr;
	}
	MaterialPackage->FullyLoad();

	UMaterialInterface* CreatedMaterial = MaterialImporter->CreateMaterialInstance(
		MaterialInfo,
		Options,
		MaterialPackage
	);

	if (CreatedMaterial)
	{
		UE_LOG(LogTemp, Verbose,
		       TEXT("GetOrCreateMaterialAsset: Successfully created material '%s' (Hash %llu) in package %s"),
		       *CreatedMaterial->GetName(), MaterialInfo.MaterialHash, *MaterialPackage->GetName());
		FScopeLock Lock(&AssetMapMutex);
		Cache.ImportedMaterials.Add(MaterialInfo.MaterialHash, CreatedMaterial);
		Cache.AddCreatedAsset(CreatedMaterial); // Add the material itself for saving
		// Textures associated with MaterialInfo should have been added via ProcessTexture calls earlier
	}
	else
	{
		UE_LOG(LogTemp, Error,
		       TEXT("GetOrCreateMaterialAsset: MaterialImporter->CreateMaterialInstance failed for '%s' (Hash %llu)"),
		       *MaterialInfo.Name, MaterialInfo.MaterialHash);
	}

	return CreatedMaterial;
}
