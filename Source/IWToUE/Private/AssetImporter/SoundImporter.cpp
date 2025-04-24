#include "AssetImporter/SoundImporter.h"

#include "Interface/IGameAssetHandler.h"
#include "Utils/CoDAssetHelper.h"
#include "WraithX/CoDAssetType.h"

bool FSoundImporter::Import(const FString& ImportPath, TSharedPtr<FCoDAsset> Asset, IGameAssetHandler* Handler,
                            FAssetImportManager* Manager)
{
	TSharedPtr<FCoDSound> SoundInfo = StaticCastSharedPtr<FCoDSound>(Asset);
	if (!SoundInfo.IsValid() || !Handler) return false;

	FWraithXSound SoundData;
	uint8 ImageFormat = 0;

	if (!Handler->ReadSoundData(SoundInfo, SoundData))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to read sound data for %s using current game handler."),
		       *SoundInfo->AssetName);
		return false;
	}

	if (!SoundData.Data.IsEmpty())
	{
		FString AssetName = FPaths::GetBaseFilename(SoundInfo->AssetName);
		AssetName = FCoDAssetNameHelper::NoIllegalSigns(AssetName);
		FString PackagePath = FPaths::Combine(ImportPath, TEXT("Sound"));
		FString FullPackagePath = FPaths::Combine(PackagePath, AssetName);
		if (SoundData.DataType == ESoundDataTypes::Wav_WithHeader)
		{
			if (USoundWave* SoundAsset = FCoDAssetHelper::CreateSoundWaveFromData(
				nullptr, AssetName, SoundData.Data, RF_Standalone | RF_Public))
			{
				// return FCoDAssetHelper::SaveObjectToPackage(SoundAsset);
			}
		}
	}
	return false;
}
