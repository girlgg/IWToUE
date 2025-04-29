#include "Importers/AnimationImporter.h"

#include "SeLogChannels.h"
#include "WraithX/CoDAssetType.h"

bool FAnimationImporter::Import(const FAssetImportContext& Context, TArray<UObject*>& OutCreatedObjects,
                                FOnAssetImportProgress ProgressDelegate)
{
	TSharedPtr<FCoDAnim> AnimInfo = Context.GetAssetInfo<FCoDAnim>();
	UE_LOG(LogITUAssetImporter, Warning, TEXT("Animation importer for '%s' is not yet fully implemented."),
	       *AnimInfo->AssetName);
	ProgressDelegate.ExecuteIfBound(
		1.0f, FText::FromString(FString::Printf(TEXT("Skipped (Not Implemented): %s"), *AnimInfo->AssetName)));
	return false;
}

UClass* FAnimationImporter::GetSupportedUClass() const
{
	return UAnimSequence::StaticClass();
}
