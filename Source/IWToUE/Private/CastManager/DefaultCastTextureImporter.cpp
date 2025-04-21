#include "CastManager/DefaultCastTextureImporter.h"

#include "CastManager/CastModel.h"
#include "EditorFramework/AssetImportData.h"
#include "Factories/TextureFactory.h"

UTexture2D* FDefaultCastTextureImporter::ImportOrLoadTexture(FCastTextureInfo& TextureInfo,
                                                             const FString& AbsoluteTextureFilePath,
                                                             const FString& AssetDestinationPath)
{
	// Construct the final asset path in the Content Browser
	FString BaseFileName = NoIllegalSigns(FPaths::GetBaseFilename(AbsoluteTextureFilePath));
	FString FinalAssetPath = FPaths::Combine(AssetDestinationPath, BaseFileName);
	FString FinalPackagePath = FPackageName::ObjectPathToPackageName(FinalAssetPath);


	// Check if asset already exists
	UPackage* ExistingPackage = FindPackage(nullptr, *FinalPackagePath);
	if (ExistingPackage)
	{
		ExistingPackage = LoadPackage(nullptr, *FinalPackagePath, LOAD_None);
	}

	UTexture2D* ExistingTexture = nullptr;
	if (ExistingPackage)
	{
		ExistingTexture = FindObject<UTexture2D>(ExistingPackage, *BaseFileName);
	}

	if (ExistingTexture)
	{
		UE_LOG(LogCast, Log, TEXT("Texture already exists, using: %s"), *ExistingTexture->GetPathName());
		TextureInfo.TextureObject = ExistingTexture; // Update the info struct
		ReportProgress(1.0f, FText::Format(
			               NSLOCTEXT("CastImporter", "TextureLoaded", "Loaded Texture {0}"),
			               FText::FromString(BaseFileName)));
		return ExistingTexture;
	}

	// Check if source file exists
	if (!FPaths::FileExists(AbsoluteTextureFilePath))
	{
		UE_LOG(LogCast, Error, TEXT("Texture source file not found: %s"), *AbsoluteTextureFilePath);
		ReportProgress(1.0f, FText::Format(
			               NSLOCTEXT("CastImporter", "TextureNotFound", "Texture File Not Found {0}"),
			               FText::FromString(BaseFileName)));
		return nullptr;
	}

	UE_LOG(LogCast, Log, TEXT("Importing texture from %s to %s"), *AbsoluteTextureFilePath, *FinalAssetPath);
	ReportProgress(0.1f, FText::Format(
		               NSLOCTEXT("CastImporter", "TextureImporting", "Importing Texture {0}"),
		               FText::FromString(BaseFileName)));

	// Import Texture using TextureFactory
	UTextureFactory* TextureFactory = NewObject<UTextureFactory>();
	TextureFactory->AddToRoot();
	TextureFactory->SuppressImportOverwriteDialog();

	bool bOperationCancelled = false;
	UPackage* TexturePackage = CreatePackage(*FinalPackagePath);
	if (!TexturePackage)
	{
		UE_LOG(LogCast, Error, TEXT("Failed to create package for texture: %s"), *FinalPackagePath);
		TextureFactory->RemoveFromRoot();
		ReportProgress(1.0f, FText::Format(
			               NSLOCTEXT("CastImporter", "TextureFailed", "Texture Failed {0}"),
			               FText::FromString(BaseFileName)));
		return nullptr;
	}
	TexturePackage->FullyLoad();

	UTexture2D* ImportedTexture = (UTexture2D*)TextureFactory->FactoryCreateFile(
		UTexture2D::StaticClass(),
		TexturePackage,
		FName(*BaseFileName),
		RF_Public | RF_Standalone | RF_Transactional,
		AbsoluteTextureFilePath,
		nullptr,
		GWarn,
		bOperationCancelled
	);

	TextureFactory->RemoveFromRoot();


	if (ImportedTexture && !bOperationCancelled)
	{
		UE_LOG(LogCast, Log, TEXT("Successfully created texture asset: %s"), *ImportedTexture->GetPathName());
		ReportProgress(0.6f, FText::Format(
			               NSLOCTEXT("CastImporter", "TextureApplyingSettings", "Applying Settings {0}"),
			               FText::FromString(BaseFileName)));

		if (ImportedTexture->GetSizeX() < 2 && ImportedTexture->GetSizeY() < 2)
		{
			UE_LOG(LogCast, Warning, TEXT("Imported texture is too small (%dx%d): %s. Deleting."),
			       ImportedTexture->GetSizeX(), ImportedTexture->GetSizeY(), *ImportedTexture->GetPathName());
			ReportProgress(1.0f, FText::Format(
				               NSLOCTEXT("CastImporter", "TextureFailedSmall", "Texture Failed (Too Small) {0}"),
				               FText::FromString(BaseFileName)));
			return nullptr;
		}

		ImportedTexture->Modify();
		ImportedTexture->PreEditChange(nullptr);

		TextureInfo.TextureType = TextureInfo.TextureType.TrimStartAndEnd();

		if (TextureInfo.TextureType == TEXT("normalMap") ||
			TextureInfo.TextureType == TEXT("normalBodyMap") ||
			TextureInfo.TextureType == TEXT("detailMap") ||
			TextureInfo.TextureType == TEXT("detailMap1") ||
			TextureInfo.TextureType == TEXT("detailMap2") ||
			TextureInfo.TextureType == TEXT("detailNormal1") ||
			TextureInfo.TextureType == TEXT("detailNormal2") ||
			TextureInfo.TextureType == TEXT("detailNormal3") ||
			TextureInfo.TextureType == TEXT("detailNormal4") ||
			TextureInfo.TextureType == TEXT("transitionNormal") ||
			TextureInfo.TextureType == TEXT("flagRippleDetailMap") ||
			TextureInfo.TextureType == TEXT("distortionMap") ||
			TextureInfo.TextureType == TEXT("crackNormalMap"))
		{
			ImportedTexture->CompressionSettings = TC_Normalmap;
			ImportedTexture->SRGB = false;
			ImportedTexture->bFlipGreenChannel = false;
		}
		else if (TextureInfo.TextureType == TEXT("aoMap") ||
			TextureInfo.TextureType == TEXT("alphaMaskMap") ||
			TextureInfo.TextureType == TEXT("alphaMask") ||
			TextureInfo.TextureType == TEXT("breakUpMap") ||
			TextureInfo.TextureType == TEXT("specularMask") ||
			TextureInfo.TextureType == TEXT("specularMaskDetail2") ||
			TextureInfo.TextureType == TEXT("tintMask") ||
			TextureInfo.TextureType == TEXT("tintBlendMask") ||
			TextureInfo.TextureType == TEXT("thicknessMap") ||
			TextureInfo.TextureType == TEXT("revealMap") ||
			TextureInfo.TextureType == TEXT("transRevealMap") ||
			TextureInfo.TextureType == TEXT("thermalHeatmap") ||
			TextureInfo.TextureType == TEXT("transGlossMap") ||
			TextureInfo.TextureType == TEXT("flickerLookupMap") ||
			TextureInfo.TextureType == TEXT("glossMap") ||
			TextureInfo.TextureType == TEXT("glossBodyMap") ||
			TextureInfo.TextureType == TEXT("glossMapDetail2"))
		{
			ImportedTexture->CompressionSettings = TC_Grayscale;
			ImportedTexture->SRGB = false;
		}
		else if (TextureInfo.TextureType == TEXT("customizeMask") ||
			TextureInfo.TextureType == TEXT("flowMap") ||
			TextureInfo.TextureType == TEXT("mixMap") ||
			TextureInfo.TextureType == TEXT("camoMaskMap") ||
			TextureInfo.TextureType == TEXT("detailNormalMask"))
		{
			ImportedTexture->CompressionSettings = TC_Masks;
			ImportedTexture->SRGB = false;
		}
		else if (TextureInfo.TextureType == TEXT("unk_semantic_0x9") ||
			TextureInfo.TextureType == TEXT("unk_semantic_0xA") ||
			TextureInfo.TextureType == TEXT("unk_semantic_0x4") ||
			TextureInfo.TextureType == TEXT("unk_semantic_0x5") ||
			TextureInfo.TextureType == TEXT("unk_semantic_0x58") ||
			TextureInfo.TextureType == TEXT("unk_semantic_0x65"))
		{
			ImportedTexture->CompressionSettings = TC_Default;
			ImportedTexture->SRGB = false;
		}
		else
		{
			ImportedTexture->CompressionSettings = TC_Default;
			ImportedTexture->SRGB = true;
		}

		ImportedTexture->PostEditChange();
		ImportedTexture->UpdateResource();

		ImportedTexture->AssetImportData->Update(AbsoluteTextureFilePath);

		TexturePackage->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(ImportedTexture);

		TextureInfo.TextureObject = ImportedTexture; // Update the info struct
		ReportProgress(1.0f, FText::Format(
			               NSLOCTEXT("CastImporter", "TextureImported", "Imported Texture {0}"),
			               FText::FromString(BaseFileName)));
		return ImportedTexture;
	}
	if (bOperationCancelled)
	{
		UE_LOG(LogCast, Warning, TEXT("Texture import cancelled by user for: %s"), *AbsoluteTextureFilePath);
	}
	else
	{
		UE_LOG(LogCast, Error, TEXT("TextureFactory failed to import file: %s"), *AbsoluteTextureFilePath);
	}
	ReportProgress(1.0f, FText::Format(
		               NSLOCTEXT("CastImporter", "TextureFailed", "Texture Failed {0}"),
		               FText::FromString(BaseFileName)));
	return nullptr;
}
