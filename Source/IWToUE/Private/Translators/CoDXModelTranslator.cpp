#include "Translators/CoDXModelTranslator.h"

#include "CastManager/CastRoot.h"
#include "Interface/IGameAssetHandler.h"
#include "WraithX/CoDAssetType.h"

bool FCoDXModelTranslator::TranslateModel(IGameAssetHandler* GameHandler, FWraithXModel& InModel, int32 LodIdx,
                                          FCastModelInfo& OutModelInfo, const FCastRoot& InSceneRoot)
{
	OutModelInfo.Name = InModel.ModelName;

	if (!InModel.ModelLods.IsValidIndex(LodIdx))
	{
		UE_LOG(LogTemp, Error, TEXT("TranslateModel: Invalid LOD index %d for model %s"), LodIdx, *InModel.ModelName);
		return false;
	}
	FWraithXModelLod& LodRef = InModel.ModelLods[LodIdx];

	for (int32 SubmeshIdx = 0; SubmeshIdx < LodRef.Submeshes.Num(); ++SubmeshIdx)
	{
		if (!LodRef.Materials.IsValidIndex(SubmeshIdx))
		{
			UE_LOG(LogTemp, Warning, TEXT("TranslateModel: Material index %d out of bounds for LOD %d of model %s."),
			       SubmeshIdx, LodIdx, *InModel.ModelName);
			continue;
		}
		FWraithXMaterial& Material = LodRef.Materials[SubmeshIdx];
		FWraithXModelSubmesh& Submesh = LodRef.Submeshes[SubmeshIdx];

		if (const uint32* FoundIndex = InSceneRoot.MaterialMap.Find(Material.MaterialHash))
		{
			Submesh.MaterialIndex = *FoundIndex;
			Submesh.MaterialHash = Material.MaterialHash;
		}
		else
		{
			UE_LOG(LogTemp, Error,
			       TEXT(
				       "TranslateModel: Material hash %llu (Material: %s) not found in pre-processed SceneRoot.MaterialMap for LOD %d of model %s. This indicates an error in the pre-processing step."
			       ),
			       Material.MaterialHash, *Material.MaterialName, LodIdx, *InModel.ModelName);
			Submesh.MaterialIndex = INDEX_NONE;
			Submesh.MaterialHash = Material.MaterialHash;
		}
	}
	if (InModel.IsModelStreamed)
	{
		GameHandler->LoadXModel(InModel, LodRef, OutModelInfo);
	}
	else
	{
	}
	return true;
}
