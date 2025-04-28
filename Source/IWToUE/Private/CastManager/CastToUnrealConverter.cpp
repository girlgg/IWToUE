#include "CastManager/CastToUnrealConverter.h"

#include "CastManager/CastAnimation.h"
#include "CastManager/CastImportOptions.h"
#include "CastManager/CastModel.h"
#include "CastManager/CastRoot.h"
#include "CastManager/CastScene.h"
#include "Interface/ICastMaterialParser.h"

UObject* FCastToUnrealConverter::Convert(FCastScene& CastScene, const FCastImportOptions& Options, UObject* InParent,
                                         FName InName, EObjectFlags Flags, const FString& OriginalFilePath,
                                         TArray<UObject*>& OutOtherCreatedObjects)
{
	FScopedSlowTask SlowTask(100, NSLOCTEXT("CastImporter", "ConvertingAssets", "Converting Cast Assets..."));
	SlowTask.MakeDialog(true);

	OutOtherCreatedObjects.Empty();

	UObject* PrimaryCreatedObject = nullptr;
	FString ParentPackagePath = InParent->IsA<UPackage>()
		                            ? InParent->GetPathName()
		                            : FPaths::GetPath(InParent->GetPathName());
	FString AssetBaseName = FPaths::GetBaseFilename(OriginalFilePath);

	// --- Material Pre-processing (Parsing & Texture Import) ---
	SlowTask.EnterProgressFrame(30, NSLOCTEXT("CastImporter", "Conv_ProcessMaterials", "Processing Materials..."));
	if (Options.bImportMaterial && CastScene.GetMaterialCount() > 0)
	{
		if (!ProcessMaterials(CastScene, Options, OriginalFilePath, ParentPackagePath))
		{
			UE_LOG(LogCast, Error, TEXT("Failed during material processing phase. Aborting conversion."));
			return nullptr;
		}
	}
	else
	{
		UE_LOG(LogCast, Log, TEXT("Skipping material processing (Not requested or no materials)."));
	}


	// --- Asset Import ---
	if (Options.bImportMesh && Options.bImportAsSkeletal && CastScene.GetSkinnedMeshNum() > 0)
	{
		SlowTask.EnterProgressFrame(65, NSLOCTEXT("CastImporter", "Conv_ImportSkM", "Importing Skeletal Mesh..."));
		UE_LOG(LogCast, Log, TEXT("Importing as Skeletal Mesh: %s"), *AssetBaseName);

		PrimaryCreatedObject = MeshImporter->ImportSkeletalMesh(
			CastScene,
			Options,
			InParent,
			FName(AssetBaseName),
			Flags,
			MaterialImporter.Get(),
			OriginalFilePath,
			OutOtherCreatedObjects);
		if (!PrimaryCreatedObject)
		{
			UE_LOG(LogCast, Error, TEXT("Failed to import Skeletal Mesh."));
		}
	}
	else if (Options.bImportMesh && CastScene.GetMeshNum() > 0)
	{
		SlowTask.EnterProgressFrame(65, NSLOCTEXT("CastImporter", "Conv_ImportStM", "Importing Static Mesh..."));
		if (!CastScene.Roots.IsEmpty() && !CastScene.Roots[0].Models.IsEmpty())
		{
			PrimaryCreatedObject = MeshImporter->ImportStaticMesh(
				CastScene,
				Options,
				InParent,
				FName(AssetBaseName),
				Flags,
				MaterialImporter.Get(),
				OriginalFilePath,
				OutOtherCreatedObjects);
		}
		else
		{
			UE_LOG(LogCast, Warning, TEXT("No models found to import as static mesh."));
		}

		if (!PrimaryCreatedObject)
		{
			UE_LOG(LogCast, Error, TEXT("Failed to import Static Mesh."));
		}
	}
	else if (Options.bImportAnimations && CastScene.HasAnimation())
	{
		SlowTask.EnterProgressFrame(65, NSLOCTEXT("CastImporter", "Conv_ImportAnim", "Importing Animation..."));

		if (!Options.Skeleton)
		{
			UE_LOG(LogCast, Error, TEXT("Cannot import animation: No Skeleton provided in options."));
			return nullptr;
		}

		int AnimIndex = 0;
		for (FCastRoot& Root : CastScene.Roots)
		{
			for (FCastAnimationInfo& AnimInfo : Root.Animations)
			{
				FString AnimAssetName = AssetBaseName;
				if (CastScene.HasAnimation() && Root.Animations.Num() > 1)
				{
					FString AnimName = ICastAssetImporter::NoIllegalSigns(AnimInfo.Name);
					if (!AnimName.IsEmpty())
					{
						AnimAssetName += TEXT("_") + ICastAssetImporter::NoIllegalSigns(AnimInfo.Name);
					}
					else
					{
						AnimAssetName += FString::Printf(TEXT("_%02d"), AnimIndex);
					}
				}

				UAnimSequence* ImportedAnim = AnimationImporter->ImportAnimation(
					AnimInfo,
					Options,
					Options.Skeleton,
					InParent,
					FName(*AnimAssetName),
					Flags
				);

				if (ImportedAnim)
				{
					if (!PrimaryCreatedObject) PrimaryCreatedObject = ImportedAnim;
					else OutOtherCreatedObjects.Add(ImportedAnim);
				}
				else
				{
					UE_LOG(LogCast, Warning, TEXT("Failed to import animation %s"), *AnimInfo.Name);
				}
				AnimIndex++;
			}
		}
	}
	else
	{
		SlowTask.EnterProgressFrame(65);
		UE_LOG(LogCast, Warning, TEXT("No suitable assets found or requested for import in %s based on options."),
		       *OriginalFilePath);
	}

	SlowTask.EnterProgressFrame(5, NSLOCTEXT("CastImporter", "Conv_Finalizing", "Finalizing Conversion..."));
	return PrimaryCreatedObject;
}

bool FCastToUnrealConverter::ProcessMaterials(FCastScene& CastScene, const FCastImportOptions& Options,
                                              const FString& OriginalFilePath, const FString& AssetImportPath)
{
	FScopedSlowTask MaterialTask(CastScene.GetMaterialCount() * 2,
	                             NSLOCTEXT("CastImporter", "ProcMat_Main", "Processing Materials..."));
	MaterialTask.MakeDialog(true);

	FString MaterialFileBasePath = FPaths::GetPath(OriginalFilePath);
	FString DefaultTextureSourcePath = FPaths::Combine(MaterialFileBasePath, TEXT("_images"));

	// --- Parse all material files ---
	UE_LOG(LogCast, Log, TEXT("Parsing %d materials..."), CastScene.GetMaterialCount());
	TArray<TTuple<FCastMaterialInfo*, FString>> MaterialsToParse;
	for (FCastRoot& Root : CastScene.Roots)
	{
		for (FCastMaterialInfo& Material : Root.Materials)
		{
			MaterialsToParse.Add(MakeTuple(&Material, MaterialFileBasePath));
		}
	}

	for (auto& MatTuple : MaterialsToParse)
	{
		MaterialTask.EnterProgressFrame(1.0f, FText::Format(
			                                NSLOCTEXT("CastImporter", "ProcMat_Parsing", "Parsing {0}"),
			                                FText::FromString(MatTuple.Get<0>()->Name)));
		if (!MaterialParser->ParseMaterialFiles(
			*MatTuple.Get<0>(), MatTuple.Get<1>(), DefaultTextureSourcePath,
			Options.TextureFormat, Options.TexturePathType, Options.GlobalMaterialPath))
		{
			UE_LOG(LogCast, Warning, TEXT("Failed to parse files for material: %s"), *MatTuple.Get<0>()->Name);
		}
	}

	// --- Collect and Import all unique textures ---
	TMap<FString, FCastTextureInfo*> UniqueTextures;
	for (FCastRoot& Root : CastScene.Roots)
	{
		for (FCastMaterialInfo& Material : Root.Materials)
		{
			for (FCastTextureInfo& Texture : Material.Textures)
			{
				if (Texture.TexturePath.IsEmpty()) continue;

				FString AbsoluteTexturePath = Texture.TexturePath;
				if (FPaths::IsRelative(AbsoluteTexturePath))
				{
					AbsoluteTexturePath = FPaths::ConvertRelativePathToFull(
						MaterialFileBasePath, AbsoluteTexturePath);
					Texture.TexturePath = AbsoluteTexturePath;
				}

				if (!UniqueTextures.Contains(AbsoluteTexturePath))
				{
					UniqueTextures.Add(AbsoluteTexturePath, &Texture);
					UE_LOG(LogCast, Verbose, TEXT("Found unique texture: %s (Type: %s)"), *AbsoluteTexturePath,
					       *Texture.TextureType);
				}
			}
		}
	}

	if (UniqueTextures.IsEmpty())
	{
		UE_LOG(LogCast, Log, TEXT("No unique textures found to import."));
		MaterialTask.EnterProgressFrame(CastScene.GetMaterialCount());
		return true;
	}

	FString TextureDestinationBasePath = FPaths::Combine(AssetImportPath, TEXT("Materials"), TEXT("Textures"));

	float TextureProgressIncrement = (float)CastScene.GetMaterialCount() / FMath::Max(1, UniqueTextures.Num());

	for (auto const& [AbsolutePath, TextureInfoPtr] : UniqueTextures)
	{
		MaterialTask.EnterProgressFrame(
			TextureProgressIncrement, FText::Format(
				NSLOCTEXT("CastImporter", "ProcMat_ImportingTex", "Importing Texture {0}"),
				FText::FromString(TextureInfoPtr->TextureName)));

		FString RelativeDir = FPaths::GetPath(AbsolutePath);
		FString TextureAssetPath = FPaths::Combine(TextureDestinationBasePath,
		                                           FPaths::GetCleanFilename(RelativeDir));


		UTexture2D* ImportedTex = TextureImporter->ImportOrLoadTexture(
			*TextureInfoPtr, AbsolutePath, TextureAssetPath);
		if (!ImportedTex)
		{
			UE_LOG(LogCast, Warning, TEXT("Failed to import or load texture: %s"), *AbsolutePath);
		}
	}


	// --- Update all TextureInfo structs with imported objects ---
	for (FCastRoot& Root : CastScene.Roots)
	{
		for (FCastMaterialInfo& Material : Root.Materials)
		{
			for (FCastTextureInfo& Texture : Material.Textures)
			{
				if (Texture.TexturePath.IsEmpty()) continue;

				if (FCastTextureInfo** FoundInfo = UniqueTextures.Find(Texture.TexturePath))
				{
					Texture.TextureObject = (*FoundInfo)->TextureObject;
				}
				else
				{
					UE_LOG(LogCast, Error,
					       TEXT("Consistency Error: Texture path %s not found in unique map after import!"),
					       *Texture.TexturePath);
				}
			}
		}
	}

	return true;
}
