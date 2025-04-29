#include "Importers/MaterialImporter.h"

#include "CastManager/CastImportOptions.h"
#include "CastManager/CastModel.h"
#include "CastManager/DefaultCastMaterialImporter.h"
#include "Interface/IGameAssetHandler.h"
#include "Utils/CoDAssetHelper.h"
#include "WraithX/CoDAssetType.h"

FMaterialImporter::FMaterialImporter()
{
	MaterialImporterInternal = MakeShared<FDefaultCastMaterialImporter>();
}

bool FMaterialImporter::Import(const FAssetImportContext& Context, TArray<UObject*>& OutCreatedObjects,
                               FOnAssetImportProgress ProgressDelegate)
{
	TSharedPtr<FCoDMaterial> MatInfo = Context.GetAssetInfo<FCoDMaterial>();
	if (!MatInfo.IsValid() || !Context.GameHandler || !Context.ImportManager || !Context.AssetCache || !
		MaterialImporterInternal.IsValid())
	{
		UE_LOG(LogITUAssetImporter, Error, TEXT("Material Import failed: Invalid context. Asset: %s"),
		       *MatInfo->AssetName);
		ProgressDelegate.ExecuteIfBound(1.0f, NSLOCTEXT("MaterialImporter", "ErrorInvalidContext",
		                                                "Import Failed: Invalid Context"));
		return false;
	}

	uint64 AssetKey = MatInfo->AssetPointer;
	FString OriginalAssetName = FPaths::GetBaseFilename(MatInfo->AssetName);
	FString SanitizedAssetName = FCoDAssetNameHelper::NoIllegalSigns(OriginalAssetName);
	if (SanitizedAssetName.IsEmpty())
	{
		SanitizedAssetName = FString::Printf(TEXT("XMaterial_ptr_%llx"), AssetKey);
	}

	ProgressDelegate.ExecuteIfBound(0.0f, FText::Format(
		                                NSLOCTEXT("MaterialImporter", "Start", "Starting Import: {0}"),
		                                FText::FromString(SanitizedAssetName)));

	// --- Check Cache ---
	{
		FScopeLock Lock(&Context.AssetCache->CacheMutex);
		if (TWeakObjectPtr<UMaterialInterface>* Found = Context.AssetCache->ImportedMaterials.Find(AssetKey))
		{
			if (Found->IsValid())
			{
				OutCreatedObjects.Add(Found->Get());
				ProgressDelegate.ExecuteIfBound(
					1.0f, NSLOCTEXT("MaterialImporter", "LoadedFromCache", "Loaded From Cache"));
				return true;
			}
			Context.AssetCache->ImportedMaterials.Remove(AssetKey);
		}
	}

	ProgressDelegate.ExecuteIfBound(0.1f, NSLOCTEXT("MaterialImporter", "ReadingData", "Reading Material Data..."));

	// --- Read WraithXMaterial data ---
	FWraithXMaterial WraithMaterial;
	if (!Context.GameHandler->ReadMaterialData(MatInfo, WraithMaterial))
	{
		UE_LOG(LogITUAssetImporter, Error, TEXT("Failed to read material data for %s using handler."),
		       *SanitizedAssetName);
		ProgressDelegate.ExecuteIfBound(1.0f, NSLOCTEXT("MaterialImporter", "ErrorReadFail",
		                                                "Import Failed: Cannot Read Data"));
		return false;
	}

	ProgressDelegate.
		ExecuteIfBound(0.2f, NSLOCTEXT("MaterialImporter", "ProcessingTextures", "Processing Textures..."));

	// --- Prepare FCastMaterialInfo (includes importing/caching textures) ---
	FCastMaterialInfo PreparedCastMaterial;
	PreparedCastMaterial.Name = SanitizedAssetName;
	PreparedCastMaterial.MaterialHash = WraithMaterial.MaterialHash;
	PreparedCastMaterial.MaterialPtr = WraithMaterial.MaterialPtr;
	PreparedCastMaterial.Textures.Reserve(WraithMaterial.Images.Num());

	FString TexturePackagePath = FPaths::Combine(Context.BaseImportPath, TEXT("Materials"), TEXT("Textures"));

	for (FWraithXImage& WraithImage : WraithMaterial.Images)
	{
		if (WraithImage.ImagePtr == 0) continue;

		UTexture2D* TextureAsset = nullptr;
		{
			FScopeLock Lock(&Context.AssetCache->CacheMutex);
			if (TWeakObjectPtr<UTexture2D>* Found = Context.AssetCache->ImportedTextures.Find(WraithImage.ImagePtr))
			{
				if (Found->IsValid()) TextureAsset = Found->Get();
				else Context.AssetCache->ImportedTextures.Remove(WraithImage.ImagePtr);
			}
		}
		if (!TextureAsset)
		{
			bool bTexturesOk = true;
			FString ImageName = FCoDAssetNameHelper::NoIllegalSigns(WraithImage.ImageName);
			if (!Context.GameHandler->ReadImageDataToTexture(WraithImage.ImagePtr, TextureAsset, ImageName,
			                                                 TexturePackagePath))
			{
				UE_LOG(LogITUAssetImporter, Warning, TEXT("Failed texture for material %s: %s"), *SanitizedAssetName,
				       *WraithImage.ImageName);
				bTexturesOk = false;
				continue;
			}
			if (TextureAsset)
			{
				Context.AssetCache->AddCreatedAsset(TextureAsset);
				FScopeLock Lock(&Context.AssetCache->CacheMutex);
				Context.AssetCache->ImportedTextures.Add(WraithImage.ImagePtr, TextureAsset);
			}
			else
			{
				bTexturesOk = false;
				continue;
			}
		}
		WraithImage.ImageObject = TextureAsset;

		FCastTextureInfo& CastTextureInfo = PreparedCastMaterial.Textures.AddDefaulted_GetRef();
		CastTextureInfo.TextureName = WraithImage.ImageName;
		CastTextureInfo.TextureObject = WraithImage.ImageObject;
		FString ParameterName = FString::Printf(TEXT("unk_semantic_0x%X"), WraithImage.SemanticHash);
		CastTextureInfo.TextureSlot = ParameterName;
		CastTextureInfo.TextureType = ParameterName;
	}

	ProgressDelegate.ExecuteIfBound(0.7f, NSLOCTEXT("MaterialImporter", "CreatingInstance",
	                                                "Creating Material Instance..."));

	// --- Create Material Instance using ICastMaterialImporter ---
	FString MaterialPackagePath = FPaths::Combine(Context.BaseImportPath, TEXT("Materials"));
	UPackage* Package = CreatePackage(*FPaths::Combine(MaterialPackagePath, SanitizedAssetName));
	if (!Package)
	{
		UE_LOG(LogITUAssetImporter, Error, TEXT("Failed package creation for material %s"), *SanitizedAssetName);
		ProgressDelegate.ExecuteIfBound(1.0f, NSLOCTEXT("MaterialImporter", "ErrorPackage",
		                                                "Import Failed: Package Error"));
		return false;
	}
	Package->FullyLoad();

	FCastImportOptions MatOptions;
	UMaterialInterface* MaterialInstance = MaterialImporterInternal->CreateMaterialInstance(
		PreparedCastMaterial, MatOptions, Package);

	if (MaterialInstance)
	{
		Context.AssetCache->AddCreatedAsset(MaterialInstance);
		OutCreatedObjects.Add(MaterialInstance);
		FScopeLock Lock(&Context.AssetCache->CacheMutex);
		Context.AssetCache->ImportedMaterials.Add(AssetKey, MaterialInstance);
		UE_LOG(LogITUAssetImporter, Log, TEXT("Successfully imported material: %s"), *MaterialInstance->GetPathName());
		ProgressDelegate.ExecuteIfBound(1.0f, NSLOCTEXT("MaterialImporter", "Success", "Import Successful"));
		return true;
	}
	UE_LOG(LogITUAssetImporter, Error, TEXT("Failed to create material instance for %s"), *SanitizedAssetName);
	ProgressDelegate.ExecuteIfBound(1.0f, NSLOCTEXT("MaterialImporter", "ErrorInstance",
	                                                "Import Failed: Instance Creation"));
	return false;
}

UClass* FMaterialImporter::GetSupportedUClass() const
{
	return UMaterialInterface::StaticClass();
}
