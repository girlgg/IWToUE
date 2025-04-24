#include "AssetImporter/ImageImporter.h"

#include "Interface/IGameAssetHandler.h"
#include "Utils/CoDAssetHelper.h"
#include "WraithX/CoDAssetType.h"

bool FImageImporter::Import(const FString& ImportPath, TSharedPtr<FCoDAsset> Asset, IGameAssetHandler* Handler,
                            FAssetImportManager* Manager)
{
	TSharedPtr<FCoDImage> ImageInfo = StaticCastSharedPtr<FCoDImage>(Asset);
	if (!ImageInfo.IsValid() || !Handler) return false;

	FString AssetName = FPaths::GetBaseFilename(ImageInfo->AssetName);
	AssetName = FCoDAssetNameHelper::NoIllegalSigns(AssetName);

	FString PackagePath = FPaths::Combine(ImportPath, TEXT("Textures"));

	UTexture2D* Texture;
	Handler->ReadImageDataToTexture(ImageInfo->AssetPointer, Texture, AssetName, PackagePath);

	FString FullPackagePath = FPaths::Combine(PackagePath, AssetName);
	if (Texture)
	{
		// return FCoDAssetHelper::SaveObjectToPackage(Texture);
	}
	UE_LOG(LogTemp, Log, TEXT("Placeholder: Create and Save Texture %s to %s"), *AssetName, *PackagePath);
	return true;
}
