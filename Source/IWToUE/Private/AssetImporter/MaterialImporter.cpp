#include "AssetImporter/MaterialImporter.h"

#include "EditorAssetLibrary.h"
#include "Interface/IGameAssetHandler.h"
#include "Utils/CoDAssetHelper.h"
#include "WraithX/CoDAssetType.h"

bool FMaterialImporter::Import(const FString& ImportPath, TSharedPtr<FCoDAsset> Asset, IGameAssetHandler* Handler,
                               FAssetImportManager* Manager)
{
	TSharedPtr<FCoDMaterial> MaterialInfo = StaticCastSharedPtr<FCoDMaterial>(Asset);
	if (!MaterialInfo.IsValid() || !Handler) return false;

	FWraithXMaterial Material;

	if (!Handler->ReadMaterialData(MaterialInfo, Material))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to read material data for %s using current game handler."),
		       *MaterialInfo->AssetName);
		return false;
	}
	FCastMaterialInfo MaterialRes;
	for (FWraithXImage& ImageInfo : Material.Images)
	{
		FCastTextureInfo& TextureInfo = MaterialRes.Textures.AddDefaulted_GetRef();

		ImageInfo.ImageName = FCoDAssetNameHelper::NoIllegalSigns(ImageInfo.ImageName);

		UTexture2D* TextureObj = nullptr;
		FString PackagePath = FPaths::Combine(ImportPath, TEXT("Textures"));

		if (!FCoDAssetNameHelper::FindAssetByName(ImageInfo.ImageName, TextureObj))
		{
			Handler->ReadImageDataToTexture(ImageInfo.ImagePtr, TextureObj, ImageInfo.ImageName, PackagePath);
		}

		TextureInfo.TextureName = ImageInfo.ImageName;
		TextureInfo.TextureObject = TextureObj;
		TextureInfo.TextureSlot = FString::Printf(TEXT("unk_semantic_0x%x"), ImageInfo.SemanticHash);
	}
	return true;
}
