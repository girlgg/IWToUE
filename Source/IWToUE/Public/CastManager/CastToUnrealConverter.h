#pragma once
#include "Interface/ICastAssetImporter.h"

struct FCastScene;
class ICastAnimationImporter;
class ICastMeshImporter;
class ICastMaterialImporter;
class ICastTextureImporter;
class ICastMaterialParser;

class FCastToUnrealConverter
{
public:
	// Constructor for Dependency Injection
	FCastToUnrealConverter(
		const TSharedPtr<ICastMaterialParser>& InMaterialParser,
		const TSharedPtr<ICastTextureImporter>& InTextureImporter,
		const TSharedPtr<ICastMaterialImporter>& InMaterialImporter,
		const TSharedPtr<ICastMeshImporter>& InMeshImporter,
		const TSharedPtr<ICastAnimationImporter>& InAnimationImporter)
		: MaterialParser(InMaterialParser), TextureImporter(InTextureImporter), MaterialImporter(InMaterialImporter)
		  , MeshImporter(InMeshImporter), AnimationImporter(InAnimationImporter)
	{
		check(MaterialParser.IsValid());
		check(TextureImporter.IsValid());
		check(MaterialImporter.IsValid());
		check(MeshImporter.IsValid());
		check(AnimationImporter.IsValid());
	}

	void SetProgressDelegate(const FOnCastImportProgress& InProgressDelegate)
	{
		ProgressDelegate = InProgressDelegate;
		// Propagate delegate to child importers
		TextureImporter->SetProgressDelegate(InProgressDelegate);
		MaterialImporter->SetProgressDelegate(InProgressDelegate);
		MeshImporter->SetProgressDelegate(InProgressDelegate);
		AnimationImporter->SetProgressDelegate(InProgressDelegate);
	}

	// Main conversion function
	UObject* Convert(FCastScene& CastScene, const FCastImportOptions& Options, UObject* InParent, FName InName,
	                 EObjectFlags Flags, const FString& OriginalFilePath, TArray<UObject*>& OutOtherCreatedObjects);

private:
	// Helper to parse materials and import their textures
	bool ProcessMaterials(FCastScene& CastScene, const FCastImportOptions& Options, const FString& OriginalFilePath,
	                      const FString& AssetImportPath);

private:
	TSharedPtr<ICastMaterialParser> MaterialParser;
	TSharedPtr<ICastTextureImporter> TextureImporter;
	TSharedPtr<ICastMaterialImporter> MaterialImporter;
	TSharedPtr<ICastMeshImporter> MeshImporter;
	TSharedPtr<ICastAnimationImporter> AnimationImporter;
	FOnCastImportProgress ProgressDelegate;
};
