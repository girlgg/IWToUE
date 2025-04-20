#include "AssetImporter/ImageImporter.h"

#include "Interface/IGameAssetHandler.h"
#include "Utils/CoDAssetHelper.h"
#include "WraithX/CoDAssetType.h"

bool FImageImporter::Import(const FString& ImportPath, TSharedPtr<FCoDAsset> Asset, IGameAssetHandler* Handler,
                            FAssetImportManager* Manager)
{
	TSharedPtr<FCoDImage> ImageInfo = StaticCastSharedPtr<FCoDImage>(Asset);
	if (!ImageInfo.IsValid() || !Handler) return false;

	TArray<uint8> ImageData;
	uint8 ImageFormat = 0;

	if (!Handler->ReadImageData(ImageInfo, ImageData, ImageFormat))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to read image data for %s using current game handler."),
		       *ImageInfo->AssetName);
		return false;
	}

	if (!ImageData.IsEmpty())
	{
		FString AssetName = FPaths::GetBaseFilename(ImageInfo->AssetName);
		FString PackagePath = FPaths::Combine(ImportPath, TEXT("Textures"));
		UTexture2D* Texture = FCoDAssetHelper::CreateTextureFromDDSData(ImageData, ImageInfo->Width, ImageInfo->Height,
		                                                                ImageFormat, AssetName);
		// if(Texture)
		// {
		// FCoDAssetHelper::SaveObjectToPackage(Texture, PackagePath, AssetName);
		// return true;
		// }
		UE_LOG(LogTemp, Log, TEXT("Placeholder: Create and Save Texture %s to %s"), *AssetName, *PackagePath);
		return true;
	}
	return false;
}
