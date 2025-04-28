#include "AssetImporter/ModelImporter.h"

#include "AssetImporter/AssetImportManager.h"
#include "CastManager/CastImportOptions.h"
#include "CastManager/CastModel.h"
#include "CastManager/CastRoot.h"
#include "CastManager/DefaultCastMaterialImporter.h"
#include "CastManager/DefaultCastMeshImporter.h"
#include "Database/CoDDatabaseService.h"
#include "Interface/IGameAssetHandler.h"
#include "Utils/CoDAssetHelper.h"
#include "WraithX/CoDAssetType.h"

bool FModelImporter::Import(const FString& ImportPath, TSharedPtr<FCoDAsset> Asset, IGameAssetHandler* Handler,
                            FAssetImportManager* Manager)
{
	TSharedPtr<FCoDModel> ModelInfo = StaticCastSharedPtr<FCoDModel>(Asset);
	if (!ModelInfo.IsValid() || !Handler || !Manager)
	{
		UE_LOG(LogTemp, Error, TEXT("Import failed: Invalid input parameters."));
		return false;
	}

	FWraithXModel GenericModel;
	if (!Handler->ReadModelData(ModelInfo, GenericModel))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to read model data for %s."), *ModelInfo->AssetName);
		return false;
	}

	FString AssetBaseName = FPaths::GetBaseFilename(ModelInfo->AssetName);
	FString ModelPackagePath = FPaths::Combine(ImportPath, TEXT("Models"), AssetBaseName);
	FString TexturePackagePath = FPaths::Combine(ModelPackagePath, TEXT("Textures"));
	FString MaterialPackagePath = FPaths::Combine(ModelPackagePath, TEXT("Materials"));

	// --- 处理纹理 ---
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
				if (Handler->ReadImageDataToTexture(Image.ImagePtr, TextureObj, Image.ImageName, TexturePackagePath))
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

	// --- 预处理材质 ---
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

	// --- 处理LoD ---

	const int32 LodCount = GenericModel.ModelLods.Num();
	SceneRoot.Models.Reserve(LodCount);
	SceneRoot.ModelLodInfo.Reserve(LodCount);

	for (int32 LodIdx = 0; LodIdx < LodCount; ++LodIdx)
	{
		FCastModelInfo ModelResult;
		if (Handler->TranslateModel(GenericModel, LodIdx, ModelResult, SceneRoot))
		{
			FCastModelLod ModelLod;
			// 目前不清楚用法
			ModelLod.Distance = GenericModel.ModelLods[LodIdx].LodDistance;
			ModelLod.MaxDistance = GenericModel.ModelLods[LodIdx].LodMaxDistance;
			ModelLod.ModelIndex = SceneRoot.Models.Add(ModelResult);
			SceneRoot.ModelLodInfo.Add(ModelLod);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to translate LOD %d for model %s."), LodIdx, *ModelInfo->AssetName);
		}
	}

	if (SceneRoot.Models.IsEmpty() || SceneRoot.ModelLodInfo.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("No valid LODs could be translated for model %s. Import failed."),
		       *ModelInfo->AssetName);
		return false;
	}

	TSharedPtr<ICastMaterialImporter> MaterialImporter = MakeShared<FDefaultCastMaterialImporter>();
	TUniquePtr<ICastMeshImporter> ModelImporter = MakeUnique<FDefaultCastMeshImporter>();
	FCastImportOptions Options;
	FString AssetName = FPaths::GetBaseFilename(ModelInfo->AssetName);
	FString PackagePath = FPaths::Combine(ImportPath, TEXT("Models"));
	TArray<UObject*> OutCreatedObjects;

	FString PackageName = FPaths::Combine(PackagePath, AssetName);
	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create package: %s"), *PackageName);
		return false;
	}
	Package->FullyLoad();
	
	if (SceneRoot.Models.Num() > 0)
	{
		FCastScene Scene;
		Scene.Roots.Add(SceneRoot);

		UObject* Mesh;

		if (!SceneRoot.Models[0].Skeletons.IsEmpty() && !SceneRoot.Models[0].Skeletons[0].Bones.IsEmpty())
		{
			Mesh = ModelImporter->ImportSkeletalMesh(
				Scene,
				Options,
				Package,
				*AssetName,
				RF_Public | RF_Standalone,
				MaterialImporter.Get(),
				TEXT(""),
				OutCreatedObjects
			);
		}
		else
		{
			Mesh = ModelImporter->ImportStaticMesh(
				Scene,
				Options,
				Package,
				*AssetName,
				RF_Public | RF_Standalone,
				MaterialImporter.Get(),
				TEXT(""),
				OutCreatedObjects
			);
		}
		return true;
	}

	return false;
}
