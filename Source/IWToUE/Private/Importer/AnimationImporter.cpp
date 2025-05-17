#include "Importers/AnimationImporter.h"

#include "SeLogChannels.h"
#include "CastManager/CastAnimation.h"
#include "CastManager/CastImportOptions.h"
#include "CastManager/DefaultCastAnimationImporter.h"
#include "Interface/IGameAssetHandler.h"
#include "Utils/CoDAssetHelper.h"
#include "WraithX/CoDAssetType.h"
#include "WraithX/WraithSettings.h"
#include "WraithX/WraithSettingsManager.h"

FAnimationImporter::FAnimationImporter()
{
	AnimImporter = MakeShared<FDefaultCastAnimationImporter>();
}

bool FAnimationImporter::Import(const FAssetImportContext& Context, TArray<UObject*>& OutCreatedObjects,
                                FOnAssetImportProgress ProgressDelegate)
{
	TSharedPtr<FCoDAnim> AnimInfo = Context.GetAssetInfo<FCoDAnim>();
	if (!AnimInfo.IsValid() || !Context.GameHandler || !Context.ImportManager || !Context.AssetCache || !AnimImporter.
		IsValid() || !Context.Settings)
	{
		UE_LOG(LogITUAssetImporter, Error,
		       TEXT("Animation Import failed: Invalid context parameters. Asset: %s. Settings: %s"),
		       AnimInfo.IsValid() ? *AnimInfo->AssetName : TEXT("Unknown (AnimInfo invalid)"),
		       Context.Settings ? TEXT("Valid") : TEXT("INVALID"));
		ProgressDelegate.ExecuteIfBound(1.0f, NSLOCTEXT("AnimationImporter", "ErrorInvalidContext",
		                                                "Import Failed: Invalid Context"));
		return false;
	}

	uint64 AssetKey = AnimInfo->AssetPointer;
	FString OriginalAssetName = FPaths::GetBaseFilename(AnimInfo->AssetName);
	FString SanitizedAssetName = FCoDAssetNameHelper::NoIllegalSigns(OriginalAssetName);
	if (SanitizedAssetName.IsEmpty())
	{
		SanitizedAssetName = FString::Printf(TEXT("XAnim_ptr_%llx"), AssetKey);
	}

	ProgressDelegate.ExecuteIfBound(0.0f, FText::Format(
		                                NSLOCTEXT("AnimationImporter", "Start", "Starting Import: {0}"),
		                                FText::FromString(SanitizedAssetName)));

	// --- 1. Check Animation Cache ---
	{
		FScopeLock Lock(&Context.AssetCache->CacheMutex);
		if (TWeakObjectPtr<UObject>* Found = Context.AssetCache->ImportedAnimations.Find(AssetKey))
		{
			if (Found->IsValid())
			{
				UObject* CachedAnimation = Found->Get();
				UE_LOG(LogITUAssetImporter, Log,
				       TEXT("Animation '%s' (Key: 0x%llX) found in cache: %s. Skipping import."),
				       *SanitizedAssetName, AssetKey, *CachedAnimation->GetPathName());
				OutCreatedObjects.Add(CachedAnimation);
				ProgressDelegate.ExecuteIfBound(
					1.0f, NSLOCTEXT("AnimationImporter", "LoadedFromCache", "Loaded From Cache"));
				return true;
			}
			UE_LOG(LogITUAssetImporter, Verbose,
			       TEXT("Animation '%s' (Key: 0x%llX) was cached but is invalid/stale. Re-importing."),
			       *SanitizedAssetName, AssetKey);
			Context.AssetCache->ImportedAnimations.Remove(AssetKey);
		}
	}
	ProgressDelegate.ExecuteIfBound(0.05f, NSLOCTEXT("AnimationImporter", "ReadingAnimData",
	                                                 "Reading Animation Data..."));

	// --- 2. Read Generic Animation Data ---
	FWraithXAnim WraithXAnimData;
	if (!Context.GameHandler->ReadAnimData(AnimInfo, WraithXAnimData))
	{
		UE_LOG(LogITUAssetImporter, Error, TEXT("Failed to read animation data for %s (Key: 0x%llX)."),
		       *SanitizedAssetName,
		       AssetKey);
		ProgressDelegate.ExecuteIfBound(1.0f, NSLOCTEXT("AnimationImporter", "ErrorReadFail",
		                                                "Import Failed: Cannot Read Data"));
		return false;
	}

	ProgressDelegate.ExecuteIfBound(0.1f, NSLOCTEXT("AnimationImporter", "TranslatingAnimData",
	                                                "Translating Animation Data..."));

	// --- 3. Translate Generic Animation Data ---
	FCastAnimationInfo GenericAnimData;
	Context.GameHandler->TranslateAnim(WraithXAnimData, GenericAnimData);

	// --- 4. Prepare Import Options and Acquire Skeleton ---

	USkeleton* TargetSkeleton = nullptr;
	const FString& SkeletonPathFromSettings = Context.Settings->Animation.TargetSkeletonPath;
	if (!SkeletonPathFromSettings.IsEmpty())
	{
		TargetSkeleton = LoadObject<USkeleton>(nullptr, *SkeletonPathFromSettings);
		if (!TargetSkeleton)
		{
			UE_LOG(LogITUAssetImporter, Warning,
			       TEXT(
				       "Failed to load target skeleton from settings path '%s' for animation '%s'. Will attempt to find/import skeleton if linked to a model."
			       ),
			       *SkeletonPathFromSettings, *SanitizedAssetName);
		}
	}
	else
	{
		UE_LOG(LogITUAssetImporter, Warning,
		       TEXT(
			       "TargetSkeletonPath is not set in Animation settings for animation '%s'. A skeleton must be specified or derivable."
		       ), *SanitizedAssetName);
	}

	FCastImportOptions Options;
	if (!TargetSkeleton)
	{
		UE_LOG(LogITUAssetImporter, Error,
		       TEXT(
			       "No valid skeleton provided or found for animation '%s'. Cannot import animation. Please check Animation.TargetSkeletonPath in settings or ensure the calling context provides a skeleton."
		       ), *SanitizedAssetName);
		ProgressDelegate.ExecuteIfBound(1.0f, NSLOCTEXT("AnimationImporter", "ErrorNoSkeleton",
		                                                "Import Failed: Skeleton Missing or Invalid"));
		return false;
	}
	Options.Skeleton = TargetSkeleton;

	ProgressDelegate.ExecuteIfBound(0.4f, FText::Format(
		                                NSLOCTEXT("AnimationImporter", "PreparingPackage",
		                                          "Preparing Package for {0} with Skeleton {1}..."),
		                                FText::FromString(SanitizedAssetName),
		                                FText::FromString(TargetSkeleton->GetName())));

	// --- 5. Create Package ---
	FString AnimRelativePath = Context.Settings->Animation.ExportDirectory;
	if (AnimRelativePath.IsEmpty())
	{
		UE_LOG(LogITUAssetImporter, Warning,
		       TEXT("Animation.ExportDirectory is not set in settings. Defaulting to 'Animations' folder."));
		AnimRelativePath = TEXT("Animations");
	}

	FString SkeletonSubFolder = FCoDAssetNameHelper::NoIllegalSigns(TargetSkeleton->GetName());
	if (SkeletonSubFolder.IsEmpty())
	{
		SkeletonSubFolder = TEXT("MiscSkeleton");
	}

	FString AnimPackageDir = FPaths::Combine(Context.BaseImportPath, AnimRelativePath, SkeletonSubFolder);
	FString FullPackageName = FPaths::Combine(AnimPackageDir, SanitizedAssetName);
	FPaths::NormalizeDirectoryName(FullPackageName);
	FPaths::MakeStandardFilename(FullPackageName);


	UPackage* Package = CreatePackage(*FullPackageName);
	if (!Package)
	{
		UE_LOG(LogITUAssetImporter, Error, TEXT("Failed to create package: %s"), *FullPackageName);
		ProgressDelegate.ExecuteIfBound(
			1.0f, NSLOCTEXT("AnimationImporter", "ErrorPackage", "Import Failed: Package Error"));
		return false;
	}
	Package->FullyLoad();

	ProgressDelegate.ExecuteIfBound(0.75f, NSLOCTEXT("AnimationImporter", "ImportingAnimAsset",
	                                                 "Importing Animation Asset..."));

	// --- 6. Import Animation using ICastAnimationImporter ---
	UAnimSequence* ImportedAnimObject = nullptr;
	EObjectFlags Flags = RF_Public | RF_Standalone;

	ImportedAnimObject = AnimImporter->ImportAnimation(GenericAnimData, Options, TargetSkeleton, Package,
	                                                   FName(*SanitizedAssetName), Flags);

	// --- 7. Finalize and Cache ---
	if (ImportedAnimObject)
	{
		UE_LOG(LogITUAssetImporter, Log, TEXT("Successfully imported animation asset: %s"),
		       *ImportedAnimObject->GetPathName());
		OutCreatedObjects.Add(ImportedAnimObject);

		Context.AssetCache->AddCreatedAsset(ImportedAnimObject);

		{
			FScopeLock Lock(&Context.AssetCache->CacheMutex);
			Context.AssetCache->ImportedAnimations.Add(AssetKey, ImportedAnimObject);
		}

		AsyncTask(ENamedThreads::GameThread, [ImportedAnimObject]()
		{
			FAssetRegistryModule::AssetCreated(ImportedAnimObject);
			ImportedAnimObject->MarkPackageDirty();
		});

		ProgressDelegate.ExecuteIfBound(1.0f, NSLOCTEXT("AnimationImporter", "Success", "Import Successful"));
		return true;
	}
	UE_LOG(LogITUAssetImporter, Error,
	       TEXT("ICastAnimationImporter failed for animation '%s' targeting skeleton '%s'."), *SanitizedAssetName,
	       *TargetSkeleton->GetPathName());
	ProgressDelegate.ExecuteIfBound(1.0f, NSLOCTEXT("AnimationImporter", "ErrorImportFail",
	                                                "Import Failed: Animation Importer Error"));
	return false;
}

UClass* FAnimationImporter::GetSupportedUClass() const
{
	return UAnimSequence::StaticClass();
}
