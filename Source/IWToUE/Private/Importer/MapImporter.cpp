#include "Importers/MapImporter.h"

#include "Animation/SkeletalMeshActor.h"
#include "CastManager/CastImportOptions.h"
#include "CastManager/CastRoot.h"
#include "CastManager/DefaultCastMaterialImporter.h"
#include "CastManager/DefaultCastMeshImporter.h"
#include "Engine/StaticMeshActor.h"
#include "Interface/IGameAssetHandler.h"
#include "Translators/CoDXModelTranslator.h"
#include "Utils/CoDAssetHelper.h"
#include "WraithX/CoDAssetType.h"

FMapImporter::FMapImporter()
{
	MeshImporterInternal = MakeShared<FDefaultCastMeshImporter>();
	MaterialImporterInternal = MakeShared<FDefaultCastMaterialImporter>();
}

bool FMapImporter::Import(const FAssetImportContext& Context, TArray<UObject*>& OutCreatedObjects,
                          FOnAssetImportProgress ProgressDelegate)
{
	TSharedPtr<FCoDMap> MapInfo = Context.GetAssetInfo<FCoDMap>();
	if (!MapInfo.IsValid() || !Context.GameHandler || !Context.ImportManager || !Context.AssetCache ||
		!MeshImporterInternal.IsValid() || !MaterialImporterInternal.IsValid())
	{
		UE_LOG(LogITUAssetImporter, Error, TEXT("Map Import failed: Invalid context parameters. Asset: %s"),
		       *MapInfo->AssetName);
		ProgressDelegate.ExecuteIfBound(1.0f, NSLOCTEXT("MapImporter", "ErrorInvalidContext",
		                                                "Import Failed: Invalid Context"));
		return false;
	}

	FString MapBaseName = FCoDAssetNameHelper::NoIllegalSigns(FPaths::GetBaseFilename(MapInfo->AssetName));
	if (MapBaseName.IsEmpty())
	{
		MapBaseName = FString::Printf(TEXT("XMap_ptr_%llx"), MapInfo->AssetPointer);
	}

	ProgressDelegate.ExecuteIfBound(
		0.0f, FText::Format(
			NSLOCTEXT("MapImporter", "Start", "Starting Map Import: {0}"), FText::FromString(MapBaseName)));

	// --- 1. Read Map Data ---
	FWraithXMap MapData;
	MapData.MapName = MapBaseName;
	if (!Context.GameHandler->ReadMapData(MapInfo, MapData))
	{
		UE_LOG(LogITUAssetImporter, Error, TEXT("Failed to read map data for %s."), *MapBaseName);
		ProgressDelegate.ExecuteIfBound(1.0f, NSLOCTEXT("MapImporter", "ErrorReadFail",
		                                                "Import Failed: Cannot Read Data"));
		return false;
	}

	// --- Define Paths ---
	FString MapSpecificBasePath = FPaths::Combine(Context.BaseImportPath, MapData.MapName);
	FString MapMeshesPackagePath = FPaths::Combine(MapSpecificBasePath, TEXT("Meshes"));
	FString MapMaterialsPackagePath = FPaths::Combine(MapSpecificBasePath, TEXT("Materials"));
	FString MapTexturesPackagePath = FPaths::Combine(MapMaterialsPackagePath, TEXT("Textures"));
	FString StaticModelsPackagePath = FPaths::Combine(MapSpecificBasePath, TEXT("StaticModels"));

	// --- 2. Pre-process Map Mesh Materials & Textures ---
	float Progress = 0.05f;
	ProgressDelegate.ExecuteIfBound(Progress, NSLOCTEXT("MapImporter", "PreprocessingMats",
	                                                    "Pre-processing Map Materials..."));
	TMap<uint64, FCastMaterialInfo> MapMeshMaterialInfos;
	int32 TotalMapMaterials = MapData.MapMeshes.Num();

	int32 ProcessedMapMaterials = 0;
	for (auto& [MeshName, MeshData] : MapData.MapMeshes)
	{
		if (MeshData.MaterialPtr == 0 && MeshData.MaterialHash == 0) continue;
		uint64 CurrentMaterialHash = MeshData.MaterialHash;
		if (MapMeshMaterialInfos.Contains(CurrentMaterialHash) && CurrentMaterialHash != 0) continue;

		FWraithXMaterial MaterialData;
		bool bReadSuccess = false;
		if (MeshData.MaterialPtr != 0)
		{
			bReadSuccess = Context.GameHandler->ReadMaterialDataFromPtr(MeshData.MaterialPtr,
			                                                            MaterialData);
			if (bReadSuccess)
			{
				CurrentMaterialHash = MaterialData.MaterialHash;
				if (MapMeshMaterialInfos.Contains(CurrentMaterialHash)) continue;
			}
			else
			{
				UE_LOG(LogITUAssetImporter, Warning, TEXT("Failed to read map material from Ptr 0x%llX for chunk %s."),
				       MeshData.MaterialPtr, *MeshName);
			}
		}
		if (!bReadSuccess && CurrentMaterialHash != 0)
		{
			MaterialData.MaterialHash = CurrentMaterialHash;
			MaterialData.MaterialName = FString::Printf(TEXT("MapMat_Hash_%llx"), CurrentMaterialHash);
			UE_LOG(LogITUAssetImporter, Warning,
			       TEXT("No MaterialPtr/ReadFail for chunk %s, Hash %llu. Info may be incomplete."),
			       *MeshName, CurrentMaterialHash);
			bReadSuccess = true;
		}
		else if (!bReadSuccess)
		{
			continue;
		}


		FCastMaterialInfo PreparedInfo;
		PrepareMaterialInfo(MeshData.MaterialPtr, MaterialData, PreparedInfo, Context, MapTexturesPackagePath);
		MapMeshMaterialInfos.Add(CurrentMaterialHash, PreparedInfo);

		ProcessedMapMaterials++;
		Progress = 0.05f + 0.25f * (static_cast<float>(ProcessedMapMaterials) / FMath::Max(1, TotalMapMaterials));
		ProgressDelegate.ExecuteIfBound(
			Progress, NSLOCTEXT("MapImporter", "PreprocessingMats", "Pre-processing Map Materials..."));
	}

	// --- 3. Import Map Mesh Chunks ---
	Progress = 0.30f;
	ProgressDelegate.ExecuteIfBound(Progress, FText::Format(
		                                NSLOCTEXT("MapImporter", "ImportingMapMeshes",
		                                          "Importing {0} Map Mesh Chunks..."),
		                                FText::AsNumber(MapData.MapMeshes.Num())));
	TArray<TWeakObjectPtr<UStaticMesh>> ImportedMapMeshes;
	ImportedMapMeshes.SetNum(MapData.MapMeshes.Num());
	int32 TotalImportTasks = MapData.MapMeshes.Num();
	TSet<uint64> UniqueStaticModelPtrSet;
	for (const FWraithMapStaticModelInstance& Instance : MapData.StaticModelInstances)
	{
		UniqueStaticModelPtrSet.Add(Instance.ModelAssetPtr);
	}
	TotalImportTasks += UniqueStaticModelPtrSet.Num();
	int32 CompletedImportTasks = 0;

	for (int32 Index = 0; Index < MapData.MapMeshes.Num(); ++Index)
	{
		const FWraithMapMeshData& MeshChunkData = MapData.MapMeshes[Index];

		uint64 MatHashToFind = MeshChunkData.MeshData.MaterialHash;
		const FCastMaterialInfo* FoundMaterialInfo = MapMeshMaterialInfos.Find(MatHashToFind);

		if (!FoundMaterialInfo && MeshChunkData.MeshData.MaterialPtr != 0)
		{
			FWraithXMaterial TempMatData;
			if (Context.GameHandler->ReadMaterialDataFromPtr(MeshChunkData.MeshData.MaterialPtr, TempMatData))
			{
				FoundMaterialInfo = MapMeshMaterialInfos.Find(TempMatData.MaterialHash);
				if (FoundMaterialInfo) MatHashToFind = TempMatData.MaterialHash;
			}
		}

		if (!FoundMaterialInfo)
		{
			UE_LOG(LogITUAssetImporter, Error,
			       TEXT("MapMeshChunk %s: Cannot find prepared material for Hash %llu. Skipping mesh import."),
			       *MeshChunkData.MeshName, MatHashToFind);
			CompletedImportTasks++;
			continue;
		}

		UStaticMesh* ImportedMesh = ImportMapMeshChunk(
			MeshChunkData,
			*FoundMaterialInfo,
			Context,
			MeshImporterInternal.Get(),
			MaterialImporterInternal.Get()
		);

		if (ImportedMesh)
		{
			ImportedMapMeshes[Index] = ImportedMesh;
			UMaterialInterface* ExpectedMaterial = nullptr;
			{
				FScopeLock Lock(&Context.AssetCache->CacheMutex);
				if (auto* FoundMat = Context.AssetCache->ImportedMaterials.Find(MatHashToFind))
				{
					if (FoundMat->IsValid()) ExpectedMaterial = FoundMat->Get();
				}
			}
			if (ExpectedMaterial && ImportedMesh->GetStaticMaterials().Num() > 0 && ImportedMesh->GetStaticMaterials()[
				0].MaterialInterface != ExpectedMaterial)
			{
				UE_LOG(LogITUAssetImporter, Warning, TEXT("Re-assigning material %s to map mesh chunk %s slot 0."),
				       *ExpectedMaterial->GetName(), *ImportedMesh->GetName());
				ImportedMesh->SetMaterial(0, ExpectedMaterial);
				Context.AssetCache->AddCreatedAsset(ImportedMesh);
			}
			else if (!ExpectedMaterial)
			{
				UE_LOG(LogITUAssetImporter, Warning,
				       TEXT("Could not find cached material for hash %llu to verify assignment on %s."), MatHashToFind,
				       *ImportedMesh->GetName());
			}
		}

		CompletedImportTasks++;
		Progress = 0.30f + 0.30f * (static_cast<float>(CompletedImportTasks) / FMath::Max(1, TotalImportTasks));
		ProgressDelegate.ExecuteIfBound(Progress, FText::Format(
			                                NSLOCTEXT("MapImporter", "ImportingMapMeshes",
			                                          "Importing {0} Map Mesh Chunks..."),
			                                FText::AsNumber(MapData.MapMeshes.Num())));
	}

	// --- 4. Import Static Model Assets ---
	Progress = 0.60f;
	ProgressDelegate.ExecuteIfBound(Progress, FText::Format(
		                                NSLOCTEXT("MapImporter", "ImportingStaticModels",
		                                          "Importing {0} Static Models..."),
		                                FText::AsNumber(UniqueStaticModelPtrSet.Num())));
	TArray<uint64> UniquePtrArray = UniqueStaticModelPtrSet.Array();
	for (int32 Index = 0; Index < UniquePtrArray.Num(); ++Index)
	{
		uint64 ModelPtr = UniquePtrArray[Index];
		ImportStaticModelAsset(ModelPtr, Context, MaterialImporterInternal.Get(), MeshImporterInternal.Get());

		CompletedImportTasks++;
		Progress = 0.60f + 0.35f * (static_cast<float>(CompletedImportTasks - MapData.MapMeshes.Num()) / FMath::Max(
			1, UniqueStaticModelPtrSet.Num()));
		ProgressDelegate.ExecuteIfBound(Progress, FText::Format(
			                                NSLOCTEXT("MapImporter", "ImportingStaticModels",
			                                          "Importing {0} Static Models..."),
			                                FText::AsNumber(UniqueStaticModelPtrSet.Num())));
	}


	// --- 5. Place Actors in Level (Scheduled on Game Thread) ---
	ProgressDelegate.ExecuteIfBound(0.95f, NSLOCTEXT("MapImporter", "SchedulingPlacement",
	                                                 "Scheduling Actor Placement..."));

	FWraithXMap MapDataCopy = MapData;
	TArray<TWeakObjectPtr<UStaticMesh>> ImportedMapMeshesCopy = ImportedMapMeshes;
	FImportedAssetsCache* CachePtr = Context.AssetCache;

	AsyncTask(ENamedThreads::GameThread, [this, MapDataCopy, ImportedMapMeshesCopy, CachePtr]() mutable
	{
		UE_LOG(LogITUAssetImporter, Log, TEXT("Executing actor placement on Game Thread..."));
		if (CachePtr)
		{
			PlaceActorsInLevel_GameThread(MapDataCopy, ImportedMapMeshesCopy, *CachePtr);
		}
		else
		{
			UE_LOG(LogITUAssetImporter, Error, TEXT("Cannot place actors: Asset Cache pointer became invalid."));
		}
	});

	ProgressDelegate.ExecuteIfBound(1.0f, NSLOCTEXT("MapImporter", "Success",
	                                                "Map Import Finished (Placement Scheduled)"));
	OutCreatedObjects = Context.AssetCache->AllCreatedAssets;
	return true;
}

UClass* FMapImporter::GetSupportedUClass() const
{
	return UWorld::StaticClass();
}

UStaticMesh* FMapImporter::ImportMapMeshChunk(const FWraithMapMeshData& MapMeshData,
                                              const FCastMaterialInfo& MaterialInfo, const FAssetImportContext& Context,
                                              ICastMeshImporter* MeshImporter,
                                              ICastMaterialImporter* MaterialImporter)
{
	if (MapMeshData.MeshData.VertexPositions.IsEmpty() || MapMeshData.MeshData.Faces.IsEmpty())
	{
		UE_LOG(LogITUAssetImporter, Warning, TEXT("Skipping map mesh chunk '%s' due to empty geometry."),
		       *MapMeshData.MeshName);
		return nullptr;
	}

	// --- Prepare a minimal FCastScene for the Mesh Importer ---
	FCastScene Scene;
	FCastRoot& Root = Scene.Roots.AddDefaulted_GetRef();

	uint32 MaterialIndex = Root.Materials.Add(MaterialInfo);
	Root.MaterialMap.Add(MaterialInfo.MaterialHash, MaterialIndex);

	FCastMeshInfo MeshDataCopy = MapMeshData.MeshData;
	MeshDataCopy.MaterialIndex = MaterialIndex; // Point to the material we just added
	MeshDataCopy.Name = FCoDAssetNameHelper::NoIllegalSigns(MapMeshData.MeshName);
	if (MeshDataCopy.Name.IsEmpty())
		MeshDataCopy.Name = FString::Printf(
			TEXT("MapChunk_Mat_%llx"), MaterialInfo.MaterialHash);

	Root.Models.AddDefaulted();
	Root.Models[0].Name = MeshDataCopy.Name;
	Root.Models[0].Meshes.Add(MoveTemp(MeshDataCopy));

	Root.ModelLodInfo.AddDefaulted();
	Root.ModelLodInfo[0].ModelIndex = 0;
	Root.ModelLodInfo[0].Distance = 0.0f;

	// --- Create Package and Import ---
	FString AssetName = Root.Models[0].Name;
	FString MapMeshesPackagePath = FPaths::Combine(Context.BaseImportPath,
	                                               FPaths::GetBaseFilename(Context.SourceAsset->AssetName),
	                                               TEXT("Meshes"));
	FString FullPackageName = FPaths::Combine(MapMeshesPackagePath, AssetName);

	UPackage* Package = CreatePackage(*FullPackageName);
	if (!Package)
	{
		UE_LOG(LogITUAssetImporter, Error, TEXT("ImportMapMeshChunk: Failed to create package: %s"), *FullPackageName);
		return nullptr;
	}
	Package->FullyLoad();

	FCastImportOptions Options;
	TArray<UObject*> CreatedAssetsForChunk;

	UStaticMesh* ImportedMesh = MeshImporter->ImportStaticMesh(
		Scene, Options, Package, FName(*AssetName), RF_Public | RF_Standalone,
		MaterialImporter,
		TEXT(""),
		CreatedAssetsForChunk);

	if (ImportedMesh)
	{
		UE_LOG(LogITUAssetImporter, Verbose, TEXT("Successfully imported map mesh chunk: %s"), *AssetName);
		Context.AssetCache->AddCreatedAsset(ImportedMesh);
		Context.AssetCache->AddCreatedAssets(CreatedAssetsForChunk);
	}
	else
	{
		UE_LOG(LogITUAssetImporter, Error, TEXT("Failed to import map mesh chunk via ICastMeshImporter: %s"),
		       *MapMeshData.MeshName);
	}

	return ImportedMesh;
}

UObject* FMapImporter::ImportStaticModelAsset(uint64 ModelPtr, const FAssetImportContext& Context,
                                              ICastMaterialImporter* MaterialImporter, ICastMeshImporter* MeshImporter)
{
	if (ModelPtr == 0) return nullptr;
	FImportedAssetsCache& Cache = *Context.AssetCache;

	// --- 1. Check Cache ---
	{
		FScopeLock Lock(&Cache.CacheMutex);
		if (TWeakObjectPtr<UObject>* Found = Cache.ImportedModels.Find(ModelPtr))
		{
			if (Found->IsValid())
			{
				UE_LOG(LogITUAssetImporter, Verbose,
				       TEXT("Static model asset with Ptr 0x%llX already imported and cached."), ModelPtr);
				return Found->Get();
			}
			Cache.ImportedModels.Remove(ModelPtr);
		}
	}

	// --- 2. Read Model Data (Requires creating a temporary FCoDModel) ---
	TSharedPtr<FCoDModel> TempModelInfo = MakeShared<FCoDModel>();
	TempModelInfo->AssetPointer = ModelPtr;
	TempModelInfo->AssetType = EWraithAssetType::Model;
	TempModelInfo->AssetName = TEXT("");

	FWraithXModel GenericModelData;
	if (!Context.GameHandler->ReadModelData(TempModelInfo, GenericModelData))
	{
		UE_LOG(LogITUAssetImporter, Error, TEXT("ImportStaticModelAsset: Failed to read model data for Ptr 0x%llX."),
		       ModelPtr);
		return nullptr;
	}
	FString SanitizedAssetName = FCoDAssetNameHelper::NoIllegalSigns(GenericModelData.ModelName);
	if (SanitizedAssetName.IsEmpty())
	{
		SanitizedAssetName = FString::Printf(TEXT("StaticModel_ptr_%llx"), ModelPtr);
		GenericModelData.ModelName = SanitizedAssetName;
	}

	UE_LOG(LogITUAssetImporter, Log, TEXT("ImportStaticModelAsset: Processing model %s (Ptr 0x%llX)"),
	       *SanitizedAssetName, ModelPtr);

	// --- 3. Process Dependencies (Textures, build Material List) ---
	FCastRoot SceneRoot;
	FString StaticModelBasePath = FPaths::Combine(Context.BaseImportPath,
	                                              FPaths::GetBaseFilename(Context.SourceAsset->AssetName),
	                                              TEXT("StaticModels"));
	FString TextureBasePath = FPaths::Combine(StaticModelBasePath, SanitizedAssetName, TEXT("Materials"),
	                                          TEXT("Textures"));

	if (!ProcessModelDependencies_MapHelper(GenericModelData, SceneRoot, Context, TextureBasePath))
	{
		UE_LOG(LogITUAssetImporter, Error, TEXT("ImportStaticModelAsset: Failed processing dependencies for model %s."),
		       *SanitizedAssetName);
		return nullptr;
	}

	// --- 4. Translate LOD Geometry ---
	bool bHasValidGeometry = false;
	SceneRoot.Models.Reserve(GenericModelData.ModelLods.Num());
	SceneRoot.ModelLodInfo.Reserve(GenericModelData.ModelLods.Num());
	for (int32 LodIdx = 0; LodIdx < GenericModelData.ModelLods.Num(); ++LodIdx)
	{
		FCastModelInfo TranslatedLodModel;
		if (FCoDXModelTranslator::TranslateModel(Context.GameHandler, GenericModelData, LodIdx, TranslatedLodModel, SceneRoot))
		{
			if (!TranslatedLodModel.Meshes.IsEmpty())
			{
				FCastModelLod ModelLodInfo;
				ModelLodInfo.Distance = GenericModelData.ModelLods[LodIdx].LodDistance;
				ModelLodInfo.MaxDistance = GenericModelData.ModelLods[LodIdx].LodMaxDistance;
				ModelLodInfo.ModelIndex = SceneRoot.Models.Add(MoveTemp(TranslatedLodModel));
				SceneRoot.ModelLodInfo.Add(ModelLodInfo);
				bHasValidGeometry = true;
			}
		}
		else
		{
			UE_LOG(LogITUAssetImporter, Warning,
			       TEXT("ImportStaticModelAsset: Failed to translate LOD %d for model %s."), LodIdx,
			       *SanitizedAssetName);
		}
	}
	if (!bHasValidGeometry)
	{
		UE_LOG(LogITUAssetImporter, Error, TEXT("ImportStaticModelAsset: No valid geometry translated for model %s."),
		       *SanitizedAssetName);
		return nullptr;
	}

	// --- Prepare data for Game Thread ---
	TSharedPtr<FCastScene> SceneData = MakeShared<FCastScene>();
	SceneData->Roots.Add(MoveTemp(SceneRoot));

	FString PackagePath = FPaths::Combine(StaticModelBasePath, SanitizedAssetName);
	FName AssetName = FName(*SanitizedAssetName);
	EObjectFlags AssetFlags = RF_Public | RF_Standalone;
	ICastMaterialImporter* CapturedMaterialImporter = MaterialImporter;
	ICastMeshImporter* CapturedMeshImporter = MeshImporter;
	FImportedAssetsCache* CapturedCache = &Cache;

	UObject* ImportedMeshObject = nullptr;
	TArray<UObject*> CreatedMeshDependencies;

	auto GameThreadTask = [=]() mutable -> UObject* {
        UE_LOG(LogITUAssetImporter, Log, TEXT("ImportStaticModelAsset: Creating package and importing mesh for %s [Game Thread]"), *AssetName.ToString());

        // --- 5. Create Package ---
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            UE_LOG(LogITUAssetImporter, Error, TEXT("ImportStaticModelAsset: Failed to create package: %s [Game Thread]"), *PackagePath);
            return nullptr;
        }
        Package->FullyLoad();

        // --- 6. Import Mesh using ICastMeshImporter ---
        FCastScene& Scene = *SceneData;

        bool bIsSkeletal = Scene.Roots.Num() > 0 &&
                           Scene.Roots[0].ModelLodInfo.Num() > 0 &&
                           Scene.Roots[0].Models.IsValidIndex(Scene.Roots[0].ModelLodInfo[0].ModelIndex) &&
                           Scene.Roots[0].Models[Scene.Roots[0].ModelLodInfo[0].ModelIndex].Skeletons.Num() > 0 &&
                           Scene.Roots[0].Models[Scene.Roots[0].ModelLodInfo[0].ModelIndex].Skeletons[0].Bones.Num() > 0;

        UObject* ResultMeshObject = nullptr;
        TArray<UObject*> LocalCreatedMeshDependencies;
        FCastImportOptions Options;

        if (bIsSkeletal)
        {
            if (!CapturedMeshImporter)
            {
                 UE_LOG(LogITUAssetImporter, Error, TEXT("ImportStaticModelAsset: MeshImporter (Skeletal) is null. [Game Thread]"));
                 return nullptr;
            }
            ResultMeshObject = CapturedMeshImporter->ImportSkeletalMesh(Scene, Options, Package, AssetName,
                                                                  AssetFlags, CapturedMaterialImporter, TEXT(""),
                                                                  LocalCreatedMeshDependencies);
        }
        else
        {
            if (!CapturedMeshImporter)
            {
                 UE_LOG(LogITUAssetImporter, Error, TEXT("ImportStaticModelAsset: MeshImporter (Static) is null. [Game Thread]"));
                 return nullptr;
            }
            ResultMeshObject = CapturedMeshImporter->ImportStaticMesh(Scene, Options, Package, AssetName,
                                                                AssetFlags, CapturedMaterialImporter, TEXT(""),
                                                                LocalCreatedMeshDependencies);
        }

        // --- 7. Finalize and Cache ---
        if (ResultMeshObject)
        {
            UE_LOG(LogITUAssetImporter, Log,
                   TEXT("ImportStaticModelAsset: Successfully imported model asset '%s' into package '%s'. [Game Thread]"),
                   *ResultMeshObject->GetName(), *Package->GetName());

            CapturedCache->AddCreatedAsset(ResultMeshObject);
            CapturedCache->AddCreatedAssets(LocalCreatedMeshDependencies);

            {
                FScopeLock Lock(&CapturedCache->CacheMutex);
                CapturedCache->ImportedModels.Add(ModelPtr, ResultMeshObject);
            }

            Package->MarkPackageDirty();

            return ResultMeshObject;
        }
        else
        {
            UE_LOG(LogITUAssetImporter, Error, TEXT("ImportStaticModelAsset: Mesh Importer failed for model: %s [Game Thread]"),
                   *AssetName.ToString());
            return nullptr;
        }
    };
	if (IsInGameThread())
	{
		ImportedMeshObject = GameThreadTask();
	}
	else
	{
		TFuture<UObject*> Future = Async(EAsyncExecution::TaskGraphMainThread, MoveTemp(GameThreadTask));
		ImportedMeshObject = Future.Get();
	}
	return ImportedMeshObject;
}

bool FMapImporter::ProcessModelDependencies_MapHelper(FWraithXModel& InOutModelData, FCastRoot& OutSceneRoot,
                                                      const FAssetImportContext& Context,
                                                      const FString& BaseTexturePath)
{
	TMap<uint64, uint32> GlobalMaterialMap;

	for (FWraithXModelLod& Lod : InOutModelData.ModelLods)
	{
		for (FWraithXMaterial& WraithMaterial : Lod.Materials)
		{
			if (GlobalMaterialMap.Contains(WraithMaterial.MaterialHash)) continue;

			for (FWraithXImage& WraithImage : WraithMaterial.Images)
			{
				if (WraithImage.ImagePtr == 0) continue;
				FString ImageName = WraithImage.ImageName;
				UTexture2D* Tex = ProcessTexture(WraithImage.ImagePtr, ImageName, Context, BaseTexturePath);
				if (!Tex)
				{
					UE_LOG(LogITUAssetImporter, Warning,
					       TEXT("ProcessModelDependencies_MapHelper: Failed texture %s for mat %s"), *ImageName,
					       *WraithMaterial.MaterialName);
				}
				WraithImage.ImageObject = Tex;
				WraithImage.ImageName = ImageName;
			}

			uint32 GlobalMaterialIndex = OutSceneRoot.Materials.Num();
			FCastMaterialInfo& CastMaterialInfo = OutSceneRoot.Materials.AddDefaulted_GetRef();
			FString MatName = FCoDAssetNameHelper::NoIllegalSigns(WraithMaterial.MaterialName);
			if (MatName.IsEmpty())
				MatName = FString::Printf(
					TEXT("StaticModelMat_Hash_%llx"), WraithMaterial.MaterialHash);
			CastMaterialInfo.Name = MatName;
			CastMaterialInfo.MaterialHash = WraithMaterial.MaterialHash;
			CastMaterialInfo.MaterialPtr = WraithMaterial.MaterialPtr;

			for (const FWraithXImage& WraithImage : WraithMaterial.Images)
			{
				if (!WraithImage.ImageObject) continue;
				FCastTextureInfo& CastTextureInfo = CastMaterialInfo.Textures.AddDefaulted_GetRef();
				CastTextureInfo.TextureName = WraithImage.ImageName;
				CastTextureInfo.TextureObject = WraithImage.ImageObject;
				FString ParamName = FString::Printf(TEXT("unk_semantic_0x%X"), WraithImage.SemanticHash);
				CastTextureInfo.TextureSlot = ParamName;
				CastTextureInfo.TextureType = ParamName;
			}
			GlobalMaterialMap.Add(WraithMaterial.MaterialHash, GlobalMaterialIndex);
		}
	}
	OutSceneRoot.MaterialMap = GlobalMaterialMap;
	return true;
}

void FMapImporter::PlaceActorsInLevel_GameThread(const FWraithXMap& MapData,
                                                 const TArray<TWeakObjectPtr<UStaticMesh>>& ImportedMapMeshes,
                                                 FImportedAssetsCache& Cache)
{
	check(IsInGameThread());

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		UE_LOG(LogITUAssetImporter, Error, TEXT("PlaceActorsInLevel_GameThread: Editor world context is invalid."));
		return;
	}
	ULevel* CurrentLevel = World->GetCurrentLevel();
	if (!CurrentLevel)
	{
		UE_LOG(LogITUAssetImporter, Error, TEXT("PlaceActorsInLevel_GameThread: Current level is invalid."));
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.OverrideLevel = CurrentLevel;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;

	// --- Place Map Meshes ---
	UE_LOG(LogITUAssetImporter, Log, TEXT("PlaceActorsInLevel_GameThread: Placing %d map mesh actors..."),
	       ImportedMapMeshes.Num());
	for (int32 i = 0; i < ImportedMapMeshes.Num(); ++i)
	{
		if (!ImportedMapMeshes[i].IsValid())
		{
			UE_LOG(LogITUAssetImporter, Warning, TEXT("Skipping placement for invalid map mesh asset at index %d."), i);
			continue;
		}
		UStaticMesh* MeshAsset = ImportedMapMeshes[i].Get();
		FString ActorBaseName = FString::Printf(TEXT("%s_Chunk_%d"), *MapData.MapName, i);
		SpawnParams.Name = MakeUniqueObjectName(CurrentLevel, AStaticMeshActor::StaticClass(), FName(*ActorBaseName));

		AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>(
			AStaticMeshActor::StaticClass(), FTransform::Identity, SpawnParams);
		if (MeshActor)
		{
			MeshActor->SetMobility(EComponentMobility::Static);
			if (UStaticMeshComponent* MeshComponent = MeshActor->GetStaticMeshComponent())
			{
				MeshComponent->SetStaticMesh(MeshAsset);
				UE_LOG(LogITUAssetImporter, Verbose, TEXT("Placed map mesh actor: %s using mesh %s"),
				       *MeshActor->GetName(), *MeshAsset->GetName());
			}
			MeshActor->SetActorLabel(ActorBaseName);
			MeshActor->RerunConstructionScripts();
			CurrentLevel->MarkPackageDirty();
		}
		else
		{
			UE_LOG(LogITUAssetImporter, Error, TEXT("Failed to spawn AStaticMeshActor for map mesh chunk %s."),
			       *MeshAsset->GetName());
		}
	}

	// --- Place Static Model Instances ---
	UE_LOG(LogITUAssetImporter, Log, TEXT("PlaceActorsInLevel_GameThread: Placing %d static model instance actors..."),
	       MapData.StaticModelInstances.Num());
	int32 InstanceCounter = 0;
	for (const FWraithMapStaticModelInstance& InstanceInfo : MapData.StaticModelInstances)
	{
		UObject* ModelAsset = nullptr;
		{
			FScopeLock Lock(&Cache.CacheMutex);
			if (TWeakObjectPtr<UObject>* FoundAssetPtr = Cache.ImportedModels.Find(InstanceInfo.ModelAssetPtr))
			{
				if (FoundAssetPtr->IsValid())
				{
					ModelAsset = FoundAssetPtr->Get();
				}
			}
		}

		if (!ModelAsset)
		{
			UE_LOG(LogITUAssetImporter, Warning,
			       TEXT("Skipping placement for instance '%s': Could not find cached asset for Ptr %llu."),
			       *InstanceInfo.InstanceName, InstanceInfo.ModelAssetPtr);
			continue;
		}

		FString BaseActorName = FCoDAssetNameHelper::NoIllegalSigns(InstanceInfo.InstanceName);
		if (BaseActorName.IsEmpty()) BaseActorName = FPaths::GetBaseFilename(ModelAsset->GetName());
		FString ActorName = FString::Printf(TEXT("%s_Inst_%d"), *BaseActorName, InstanceCounter++);
		SpawnParams.Name = MakeUniqueObjectName(CurrentLevel, AActor::StaticClass(), FName(*ActorName));

		AActor* PlacedActor = nullptr;
		if (UStaticMesh* StaticMeshAsset = Cast<UStaticMesh>(ModelAsset))
		{
			SpawnParams.Name = MakeUniqueObjectName(CurrentLevel, AStaticMeshActor::StaticClass(), FName(*ActorName));

			AStaticMeshActor* StaticMeshActor = World->SpawnActor<AStaticMeshActor>(
				AStaticMeshActor::StaticClass(), InstanceInfo.Transform, SpawnParams);
			if (StaticMeshActor)
			{
				StaticMeshActor->SetMobility(EComponentMobility::Static);
				if (UStaticMeshComponent* MeshComponent = StaticMeshActor->GetStaticMeshComponent())
				{
					MeshComponent->SetStaticMesh(StaticMeshAsset);
				}
				StaticMeshActor->SetActorLabel(ActorName);
				StaticMeshActor->RerunConstructionScripts();
				PlacedActor = StaticMeshActor;
				UE_LOG(LogITUAssetImporter, Verbose, TEXT("Placed static model actor: %s using mesh %s"),
				       *StaticMeshActor->GetName(), *StaticMeshAsset->GetName());
			}
			else
			{
				UE_LOG(LogITUAssetImporter, Error, TEXT("Failed to spawn AStaticMeshActor for instance: %s"),
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
				if (USkeletalMeshComponent* MeshComponent = SkeletalMeshActor->GetSkeletalMeshComponent())
				{
					MeshComponent->SetSkeletalMesh(SkeletalMeshAsset);
				}
				SkeletalMeshActor->SetActorLabel(ActorName);
				SkeletalMeshActor->RerunConstructionScripts();
				PlacedActor = SkeletalMeshActor;
				UE_LOG(LogITUAssetImporter, Verbose, TEXT("Placed skeletal model actor: %s using mesh %s"),
				       *SkeletalMeshActor->GetName(), *SkeletalMeshAsset->GetName());
			}
			else
			{
				UE_LOG(LogITUAssetImporter, Error, TEXT("Failed to spawn ASkeletalMeshActor for instance: %s"),
				       *InstanceInfo.InstanceName);
			}
		}
		else
		{
			UE_LOG(LogITUAssetImporter, Error, TEXT("Asset for Ptr %llx (Name: %s) is not Static or Skeletal Mesh: %s"),
			       InstanceInfo.ModelAssetPtr, *InstanceInfo.InstanceName, *ModelAsset->GetPathName());
		}
		if (PlacedActor)
		{
			CurrentLevel->MarkPackageDirty();
		}
	}

	GEditor->RedrawLevelEditingViewports(true);
	UE_LOG(LogITUAssetImporter, Log, TEXT("PlaceActorsInLevel_GameThread: Finished placing actors."));
}

void FMapImporter::PrepareMaterialInfo(const uint64 MaterialPtr, FWraithXMaterial& InMaterialData,
                                       FCastMaterialInfo& OutMaterialInfo, const FAssetImportContext& Context,
                                       const FString& BaseTexturePath)
{
	OutMaterialInfo.Name = FCoDAssetNameHelper::NoIllegalSigns(InMaterialData.MaterialName);
	if (OutMaterialInfo.Name.IsEmpty())
		OutMaterialInfo.Name = FString::Printf(
			TEXT("MapMat_Hash_%llx"), InMaterialData.MaterialHash);
	OutMaterialInfo.MaterialHash = InMaterialData.MaterialHash;
	OutMaterialInfo.MaterialPtr = MaterialPtr;
	OutMaterialInfo.Textures.Reserve(InMaterialData.Images.Num());

	for (FWraithXImage& ImageInfo : InMaterialData.Images)
	{
		FString ImageName = ImageInfo.ImageName;

		if (UTexture2D* TextureObj = ProcessTexture(ImageInfo.ImagePtr, ImageName, Context, BaseTexturePath))
		{
			FCastTextureInfo& TextureInfo = OutMaterialInfo.Textures.AddDefaulted_GetRef();
			TextureInfo.TextureName = ImageName;
			TextureInfo.TextureObject = TextureObj;
			FString ParamName = FString::Printf(TEXT("unk_semantic_0x%x"), ImageInfo.SemanticHash);
			TextureInfo.TextureSlot = ParamName;
			TextureInfo.TextureType = ParamName;
			ImageInfo.ImageObject = TextureObj;
		}
	}
}

UTexture2D* FMapImporter::ProcessTexture(uint64 ImagePtr, FString& InOutImageName, const FAssetImportContext& Context,
                                         const FString& BaseTexturePath)
{
	if (ImagePtr == 0) return nullptr;
	FImportedAssetsCache& Cache = *Context.AssetCache;

	// --- Check Cache ---
	{
		FScopeLock Lock(&Cache.CacheMutex);
		if (TWeakObjectPtr<UTexture2D>* Found = Cache.ImportedTextures.Find(ImagePtr))
		{
			if (Found->IsValid()) { return Found->Get(); }
			Cache.ImportedTextures.Remove(ImagePtr);
		}
	}

	// --- Import ---
	InOutImageName = FCoDAssetNameHelper::NoIllegalSigns(InOutImageName);
	UTexture2D* TextureObj = nullptr;
	if (Context.GameHandler->ReadImageDataToTexture(ImagePtr, TextureObj, InOutImageName, BaseTexturePath))
	{
		if (TextureObj)
		{
			Cache.AddCreatedAsset(TextureObj);
			FScopeLock Lock(&Cache.CacheMutex);
			Cache.ImportedTextures.Add(ImagePtr, TextureObj);
		}
		else
		{
			UE_LOG(LogITUAssetImporter, Warning,
			       TEXT("ProcessTexture: Handler returned null texture for Ptr 0x%llX, Name '%s'"), ImagePtr,
			       *InOutImageName);
		}
	}
	else
	{
		UE_LOG(LogITUAssetImporter, Error, TEXT("ProcessTexture: Handler failed for Ptr 0x%llX, Name '%s'"), ImagePtr,
		       *InOutImageName);
	}
	return TextureObj;
}

UMaterialInterface* FMapImporter::GetOrCreateMaterialAsset(const FCastMaterialInfo& MaterialInfo,
                                                           ICastMaterialImporter* MaterialImporter,
                                                           const FString& PreferredPackagePath,
                                                           const FAssetImportContext& Context)
{
	if (MaterialInfo.MaterialHash == 0) return nullptr;
	FImportedAssetsCache& Cache = *Context.AssetCache;

	// --- Check Cache ---
	{
		FScopeLock Lock(&Cache.CacheMutex);
		if (TWeakObjectPtr<UMaterialInterface>* Found = Cache.ImportedMaterials.Find(MaterialInfo.MaterialHash))
		{
			if (Found->IsValid()) { return Found->Get(); }
			Cache.ImportedMaterials.Remove(MaterialInfo.MaterialHash);
		}
	}

	// --- Create ---
	FString MaterialAssetName = FCoDAssetNameHelper::NoIllegalSigns(MaterialInfo.Name);
	if (MaterialAssetName.IsEmpty())
		MaterialAssetName = FString::Printf(
			TEXT("MapMI_Hash_%llu"), MaterialInfo.MaterialHash);

	FString FullMaterialPackageName = FPaths::Combine(PreferredPackagePath, MaterialAssetName);
	UPackage* MaterialPackage = CreatePackage(*FullMaterialPackageName);
	if (!MaterialPackage)
	{
		UE_LOG(LogITUAssetImporter, Error, TEXT("GetOrCreateMaterialAsset (Map): Failed package %s"),
		       *FullMaterialPackageName);
		return nullptr;
	}
	MaterialPackage->FullyLoad();

	const FCastImportOptions MatOptions;
	UMaterialInterface* CreatedMaterial = MaterialImporter->CreateMaterialInstance(
		MaterialInfo, MatOptions, MaterialPackage);

	if (CreatedMaterial)
	{
		UE_LOG(LogITUAssetImporter, Verbose, TEXT("GetOrCreateMaterialAsset (Map): Created material '%s' (Hash %llu)"),
		       *CreatedMaterial->GetName(), MaterialInfo.MaterialHash);
		FScopeLock Lock(&Cache.CacheMutex);
		Cache.ImportedMaterials.Add(MaterialInfo.MaterialHash, CreatedMaterial);
		Cache.AddCreatedAsset(CreatedMaterial);
	}
	else
	{
		UE_LOG(LogITUAssetImporter, Error,
		       TEXT("GetOrCreateMaterialAsset (Map): CreateMaterialInstance failed for '%s' (Hash %llu)"),
		       *MaterialInfo.Name, MaterialInfo.MaterialHash);
	}
	return CreatedMaterial;
}
