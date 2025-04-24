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
				else
				{
					UE_LOG(LogTemp, Error, TEXT("Failed to read/create texture data for image %s (Ptr: 0x%llX)"),
					       *Image.ImageName, Image.ImagePtr);
				}
			}
		}
	}

	FCastRoot SceneRoot;
	const int32 LodCount = GenericModel.ModelLods.Num();
	SceneRoot.Models.Reserve(LodCount);
	SceneRoot.ModelLodInfo.Reserve(LodCount);

	for (int32 LodIdx = 0; LodIdx < LodCount; ++LodIdx)
	{
		FCastModelInfo ModelResult;
		if (Handler->TranslateModel(GenericModel, LodIdx, ModelResult))
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

	TSharedPtr<ICastMaterialImporter> MaterialImporter = MakeShared<FDefaultCastMaterialImporter>();
	TUniquePtr<ICastMeshImporter> ModelImporter = MakeUnique<FDefaultCastMeshImporter>();
	FCastImportOptions Options;
	FString AssetName = FPaths::GetBaseFilename(ModelInfo->AssetName);
	FString PackagePath = FPaths::Combine(ImportPath, TEXT("Models"));
	TArray<UObject*> OutCreatedObjects;

	FString PackageName = FPaths::Combine(PackagePath, AssetName);
	UPackage* Package = CreatePackage(*PackageName);

	if (SceneRoot.Models.Num() > 0)
	{
		FCastScene Scene;
		Scene.Roots.Add(SceneRoot);

		UObject* Mesh;

		if (!SceneRoot.Models[0].Skeletons.IsEmpty())
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
