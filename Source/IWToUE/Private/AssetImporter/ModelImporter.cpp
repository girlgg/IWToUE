#include "AssetImporter/ModelImporter.h"

#include "AssetImporter/AssetImportManager.h"
#include "CastManager/CastModel.h"
#include "CastManager/CastRoot.h"
#include "Interface/IGameAssetHandler.h"
#include "WraithX/CoDAssetType.h"

bool FModelImporter::Import(const FString& ImportPath, TSharedPtr<FCoDAsset> Asset, IGameAssetHandler* Handler,
                            FAssetImportManager* Manager)
{
	TSharedPtr<FCoDModel> ModelInfo = StaticCastSharedPtr<FCoDModel>(Asset);
	if (!ModelInfo.IsValid() || !Handler || !Manager) return false;

	FWraithXModel GenericModel;
	if (!Handler->ReadModelData(ModelInfo, GenericModel))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to read model data for %s."), *ModelInfo->AssetName);
		return false;
	}

	for (FWraithXModelLod& LodModel : GenericModel.ModelLods)
	{
		for (FWraithXMaterial& Material : LodModel.Materials)
		{
			for (const FWraithXImage& Image : Material.Images)
			{
				// 导出材质图像名
				// 导出材质图像
				// 注意重复性校验
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
			ModelLod.Distance = GenericModel.ModelLods[LodIdx].LodDistance;
			ModelLod.MaxDistance = GenericModel.ModelLods[LodIdx].LodMaxDistance;

			ModelLod.ModelIndex = SceneRoot.Models.Add(ModelResult); // Add translated model data
			SceneRoot.ModelLodInfo.Add(ModelLod); // Add LOD info
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to translate LOD %d for model %s."), LodIdx, *ModelInfo->AssetName);
		}
	}

	if (SceneRoot.Models.Num() > 0)
	{
		FString AssetName = FPaths::GetBaseFilename(ModelInfo->AssetName);
		FString PackagePath = FPaths::Combine(ImportPath, TEXT("Models"));
		// UObject* MeshAsset = FCoDAssetHelper::CreateMeshFromCastData(SceneRoot, AssetName); // Your helper function
		// if(MeshAsset)
		// {
		//     FCoDAssetHelper::SaveObjectToPackage(MeshAsset, PackagePath, AssetName);
		//     return true;
		// }
		return true;
	}

	return false;
}
