#include "Importers/ImageImporter.h"

#include "SeLogChannels.h"
#include "Interface/IGameAssetHandler.h"
#include "Utils/CoDAssetHelper.h"
#include "WraithX/CoDAssetType.h"
#include "WraithX/WraithSettings.h"

bool FImageImporter::Import(const FAssetImportContext& Context, TArray<UObject*>& OutCreatedObjects,
                            FOnAssetImportProgress ProgressDelegate)
{
	TSharedPtr<FCoDImage> ImageInfo = Context.GetAssetInfo<FCoDImage>();
	if (!ImageInfo.IsValid() || !Context.GameHandler || !Context.ImportManager || !Context.AssetCache)
	{
		UE_LOG(LogITUAssetImporter, Error,
		       TEXT(
			       "Texture Import failed: Invalid context parameters (ImageInfo, Handler, Manager, or Cache is null). Asset: %s"
		       ), Context.SourceAsset.IsValid() ? *Context.SourceAsset->AssetName : TEXT("Unknown"));
		ProgressDelegate.ExecuteIfBound(1.0f, NSLOCTEXT("TextureImporter", "ErrorInvalidContext",
		                                                "Import Failed: Invalid Context"));
		return false;
	}

	uint64 AssetKey = ImageInfo->AssetPointer;
	FString OriginalAssetName = FPaths::GetBaseFilename(ImageInfo->AssetName);
	FString SanitizedAssetName = FCoDAssetNameHelper::NoIllegalSigns(OriginalAssetName);
	if (SanitizedAssetName.IsEmpty())
	{
		SanitizedAssetName = FString::Printf(TEXT("XImage_ptr_%llx"), AssetKey);
		UE_LOG(LogITUAssetImporter, Warning, TEXT("Texture had invalid/empty name, using generated: %s"),
		       *SanitizedAssetName);
	}

	ProgressDelegate.ExecuteIfBound(0.0f, FText::Format(
		                                NSLOCTEXT("TextureImporter", "Start", "Starting Import: {0}"),
		                                FText::FromString(SanitizedAssetName)));

	// --- 1. Check Cache ---
	{
		FScopeLock Lock(&Context.AssetCache->CacheMutex);
		if (TWeakObjectPtr<UTexture2D>* Found = Context.AssetCache->ImportedTextures.Find(AssetKey))
		{
			if (Found->IsValid())
			{
				UTexture2D* CachedTexture = Found->Get();
				UE_LOG(LogITUAssetImporter, Log,
				       TEXT("Texture '%s' (Key: 0x%llX) found in cache: %s. Skipping import."),
				       *SanitizedAssetName, AssetKey, *CachedTexture->GetPathName());
				OutCreatedObjects.Add(CachedTexture);
				ProgressDelegate.ExecuteIfBound(
					1.0f, NSLOCTEXT("TextureImporter", "LoadedFromCache", "Loaded From Cache"));
				return true;
			}
			UE_LOG(LogITUAssetImporter, Verbose,
			       TEXT("Texture '%s' (Key: 0x%llX) was cached but is invalid/stale. Re-importing."),
			       *SanitizedAssetName, AssetKey);
			Context.AssetCache->ImportedTextures.Remove(AssetKey);
		}
	}

	FString RelativePath;
	if (Context.Settings->Texture.bUseGlobalTexturePath && !Context.Settings->Texture.GlobalTexturePath.IsEmpty())
	{
		RelativePath = Context.Settings->Texture.GlobalTexturePath;
	}
	else if (!Context.Settings->Texture.ExportDirectory.IsEmpty())
	{
		RelativePath = Context.Settings->Texture.ExportDirectory;
	}
	else
	{
		RelativePath = TEXT("Textures");
	}

	FString FinalTextureBasePath = FPaths::Combine(Context.BaseImportPath, RelativePath);
	FPaths::NormalizeDirectoryName(FinalTextureBasePath);
	FString TexturePackageDir = FinalTextureBasePath;

	ProgressDelegate.ExecuteIfBound(0.1f, NSLOCTEXT("TextureImporter", "ReadingData", "Requesting Texture Data..."));

	// --- 2. Read/Create Texture via Handler ---
	UTexture2D* Texture = nullptr;
	FString UsedAssetName = SanitizedAssetName;

	if (!Context.GameHandler->ReadImageDataToTexture(AssetKey, Texture, UsedAssetName, TexturePackageDir))
	{
		UE_LOG(LogITUAssetImporter, Error, TEXT("Handler failed to read/create texture data for %s (Key: 0x%llX)."),
		       *SanitizedAssetName, AssetKey);
		ProgressDelegate.ExecuteIfBound(1.0f, NSLOCTEXT("TextureImporter", "ErrorReadFail",
		                                                "Import Failed: Cannot Read/Create Data"));
		return false;
	}

	if (!Texture)
	{
		UE_LOG(LogITUAssetImporter, Error,
		       TEXT("Handler reported success but returned null texture for %s (Key: 0x%llX)."), *SanitizedAssetName,
		       AssetKey);
		ProgressDelegate.ExecuteIfBound(1.0f, NSLOCTEXT("TextureImporter", "ErrorNullTexture",
		                                                "Import Failed: Handler Returned Null"));
		return false;
	}

	ProgressDelegate.ExecuteIfBound(0.8f, FText::Format(
		                                NSLOCTEXT("TextureImporter", "Finalizing", "Finalizing {0}..."),
		                                FText::FromString(UsedAssetName)));

	// --- 3. Finalize and Update Cache ---
	UE_LOG(LogITUAssetImporter, Log, TEXT("Successfully created/loaded texture: %s"), *Texture->GetPathName());

	{
		FScopeLock Lock(&Context.AssetCache->CacheMutex);
		Context.AssetCache->ImportedTextures.Add(AssetKey, Texture);
	}

	Context.AssetCache->AddCreatedAsset(Texture);

	OutCreatedObjects.Add(Texture);

	ProgressDelegate.ExecuteIfBound(1.0f, NSLOCTEXT("TextureImporter", "Success", "Import Successful"));
	return true;
}

UClass* FImageImporter::GetSupportedUClass() const
{
	return UTexture2D::StaticClass();
}
