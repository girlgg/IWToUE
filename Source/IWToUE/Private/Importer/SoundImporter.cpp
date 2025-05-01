#include "Importers/SoundImporter.h"

#include "SeLogChannels.h"
#include "Interface/IGameAssetHandler.h"
#include "Utils/CoDAssetHelper.h"
#include "WraithX/CoDAssetType.h"

bool FSoundImporter::Import(const FAssetImportContext& Context, TArray<UObject*>& OutCreatedObjects,
                            FOnAssetImportProgress ProgressDelegate)
{
	TSharedPtr<FCoDSound> SoundInfo = Context.GetAssetInfo<FCoDSound>();
	if (!SoundInfo.IsValid() || !Context.GameHandler || !Context.ImportManager || !Context.AssetCache)
	{
		UE_LOG(LogITUAssetImporter, Error, TEXT("Sound Import failed: Invalid context. Asset: %s"),
		       *SoundInfo->AssetName);
		ProgressDelegate.ExecuteIfBound(1.0f, NSLOCTEXT("SoundImporter", "ErrorInvalidContext",
		                                                "Import Failed: Invalid Context"));
		return false;
	}

	FString OriginalAssetName = FPaths::GetBaseFilename(SoundInfo->AssetName);
	FString SanitizedAssetName = FCoDAssetNameHelper::NoIllegalSigns(OriginalAssetName);
	if (SanitizedAssetName.IsEmpty())
	{
		SanitizedAssetName = FString::Printf(TEXT("XSound_ptr_%llx"), SoundInfo->AssetPointer);
	}

	ProgressDelegate.ExecuteIfBound(0.0f, FText::Format(
		                                NSLOCTEXT("SoundImporter", "Start", "Starting Import: {0}"),
		                                FText::FromString(SanitizedAssetName)));

	ProgressDelegate.ExecuteIfBound(0.1f, NSLOCTEXT("SoundImporter", "ReadingData", "Reading Sound Data..."));
	FWraithXSound SoundData;
	if (!Context.GameHandler->ReadSoundData(SoundInfo, SoundData))
	{
		UE_LOG(LogITUAssetImporter, Error, TEXT("Failed to read sound data for %s using current game handler."),
		       *SanitizedAssetName);
		ProgressDelegate.ExecuteIfBound(1.0f, NSLOCTEXT("SoundImporter", "ErrorReadFail",
		                                                "Import Failed: Cannot Read Data"));
		return false;
	}

	if (SoundData.Data.IsEmpty())
	{
		UE_LOG(LogITUAssetImporter, Warning, TEXT("Sound data for %s is empty after reading."), *SanitizedAssetName);
		ProgressDelegate.
			ExecuteIfBound(1.0f, NSLOCTEXT("SoundImporter", "ErrorEmptyData", "Import Failed: Empty Data"));
		return false;
	}

	ProgressDelegate.ExecuteIfBound(0.7f, NSLOCTEXT("SoundImporter", "CreatingAsset", "Creating Sound Asset..."));

	if (SoundData.DataType == ESoundDataTypes::Wav_WithHeader || SoundData.DataType == ESoundDataTypes::Wav_NeedsHeader)
	{
		USoundWave* SoundAsset;
		FString PackagePath = FPaths::Combine(Context.BaseImportPath, TEXT("Audio")); // Sound -> Audio
		FString FullPackagePath = FPaths::Combine(PackagePath, SanitizedAssetName);
		UPackage* Package = CreatePackage(*FullPackagePath);
		if (!Package)
		{
			UE_LOG(LogITUAssetImporter, Error, TEXT("Failed to create package for sound: %s"), *FullPackagePath);
			ProgressDelegate.ExecuteIfBound(
				1.0f, NSLOCTEXT("SoundImporter", "ErrorPackage", "Import Failed: Package Error"));
			return false;
		}
		Package->FullyLoad();

		if (SoundData.DataType == ESoundDataTypes::Wav_WithHeader)
		{
			SoundAsset = FCoDAssetHelper::CreateSoundWaveFromData(
				Package, SanitizedAssetName, SoundData.Data, RF_Public | RF_Standalone | RF_Transactional);
		}
		else
		{
			UE_LOG(LogITUAssetImporter, Warning,
			       TEXT("Sound %s needs WAV header, but adding logic is not implemented here yet."),
			       *SanitizedAssetName);
			ProgressDelegate.ExecuteIfBound(
				1.0f, NSLOCTEXT("SoundImporter", "ErrorNeedsHeader", "Import Failed: Needs Header"));
			return false; // Or attempt to add header
		}

		if (SoundAsset)
		{
			Context.AssetCache->AddCreatedAsset(SoundAsset);
			OutCreatedObjects.Add(SoundAsset);
			UE_LOG(LogITUAssetImporter, Log, TEXT("Successfully imported sound: %s"), *SoundAsset->GetPathName());
			ProgressDelegate.ExecuteIfBound(1.0f, NSLOCTEXT("SoundImporter", "Success", "Import Successful"));
			return true;
		}
		UE_LOG(LogITUAssetImporter, Error, TEXT("Failed to create SoundWave asset for %s."), *SanitizedAssetName);
		ProgressDelegate.ExecuteIfBound(
			1.0f, NSLOCTEXT("SoundImporter", "ErrorAssetCreation", "Import Failed: Asset Creation"));
		return false;
	}
	UE_LOG(LogITUAssetImporter, Warning, TEXT("Unsupported sound data type (%d) for %s."), (int)SoundData.DataType,
	       *SanitizedAssetName);
	ProgressDelegate.ExecuteIfBound(1.0f, NSLOCTEXT("SoundImporter", "ErrorUnsupportedType",
	                                                "Import Failed: Unsupported Type"));
	return false;
}

UClass* FSoundImporter::GetSupportedUClass() const
{
	return USoundWave::StaticClass();
}
