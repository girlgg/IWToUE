#include "Utils/CastImporter.h"

#include "AssetToolsModule.h"
#include "IMeshBuilderModule.h"
#include "MaterialDomain.h"
#include "ObjectTools.h"
#include "PhysicsAssetUtils.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "SeLogChannels.h"
#include "SkeletalMeshAttributes.h"
#include "StaticMeshAttributes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "CastManager/CastAnimation.h"
#include "CastManager/CastModel.h"
#include "CastManager/CastNode.h"
#include "CastManager/CastRoot.h"
#include "CastManager/CastScene.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Interfaces/IMainFrameModule.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Widgets/CastOptionWindow.h"
#include "Windows/WindowsPlatformApplicationMisc.h"
#include "FileHelpers.h"
#include "Factories/TextureFactory.h"

#define LOCTEXT_NAMESPACE "CastMainImport"

TSharedPtr<FCastImporter> FCastImporter::StaticInstance;

FCastImporter::FCastImporter()
{
	ImportOptions = MakeShared<FCastImportOptions>();
	FMemory::Memzero(*ImportOptions);

	CurPhase = NOTSTARTED;

	FCoreDelegates::OnPreExit.AddLambda([]()
	{
		GetInstance()->CleanUp();
	});

	CastManager = new FCastManager;
}

FCastImporter::~FCastImporter()
{
	CleanUp();
	if (CastManager)
	{
		CastManager->Destroy();
		delete CastManager;
		CastManager = nullptr;
	}
}

void FCastImporter::CleanUp()
{
	ReleaseScene();

	ImportOptions = nullptr;
}

void FCastImporter::ReleaseScene()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FCastImporter::ReleaseScene)

	CreatedObjects.Empty();
	CurPhase = NOTSTARTED;
}

int32 FCastImporter::GetImportType(const FString& InFilename)
{
	int32 Result = -1;
	FString Filename = InFilename;

	if (OpenFile(Filename))
	{
		if (GetSceneInfo(Filename))
		{
			if (SceneInfo.SkinnedMeshNum > 0)
			{
				Result = 1;
			}
			else if (SceneInfo.TotalGeometryNum > 0)
			{
				Result = 0;
			}
		}

		if (SceneInfo.bHasAnimation)
		{
			Result = 2;
		}
	}

	return Result;
}

bool FCastImporter::OpenFile(FString Filename)
{
	bool Result = true;

	if (CurPhase != NOTSTARTED)
	{
		return false;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(FCastImporter::OpenFile);

	GWarn->BeginSlowTask(LOCTEXT("OpeningFile", "Reading File"), true);
	GWarn->StatusForceUpdate(20, 100, LOCTEXT("OpeningFile", "Reading File"));

	ClearAllCaches();

	const bool bImportStatus = CastManager->Initialize(Filename);

	GWarn->StatusForceUpdate(100, 100, LOCTEXT("OpeningFile", "Reading File"));
	GWarn->EndSlowTask();

	if (!bImportStatus)
	{
		UE_LOG(LogCast, Error, TEXT("Call to FbxImporter::Initialize() failed."));
		return false;
	}

	Md5Hash = FMD5Hash::HashFile(*Filename);

	CurPhase = FILEOPENED;

	return Result;
}

bool FCastImporter::GetSceneInfo(FString Filename)
{
	bool Result = true;
	GWarn->BeginSlowTask(NSLOCTEXT("CastImporter", "BeginGetSceneInfoTask", "Parse Cast file to get scene info"), true);

	switch (CurPhase)
	{
	case NOTSTARTED:
		if (!OpenFile(Filename))
		{
			Result = false;
			break;
		}
		GWarn->UpdateProgress(40, 100);
	case FILEOPENED:
		if (!ImportFile(Filename))
		{
			Result = false;
			break;
		}
		GWarn->UpdateProgress(90, 100);
	default:
		break;
	}

	if (Result)
	{
		SceneInfo.TotalMaterialNum = CastManager->Scene->GetMaterialCount();
		SceneInfo.TotalTextureNum = CastManager->Scene->GetTextureCount();
		SceneInfo.bHasAnimation = CastManager->Scene->HasAnimation();
		SceneInfo.FrameRate = CastManager->Scene->GetAnimFramerate();
		SceneInfo.SkinnedMeshNum = CastManager->Scene->GetSkinnedMeshNum();
		SceneInfo.TotalGeometryNum = CastManager->Scene->GetMeshNum();
	}

	GWarn->EndSlowTask();
	return Result;
}

bool FCastImporter::ImportFile(FString Filename)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FFbxImporter::ImportFile);

	bool Result = true;

	FileBasePath = FPaths::GetPath(Filename);

	bool bStatus = CastManager->Import();

	UE_LOG(LogCast, Log, TEXT("Loading Cast Scene from %s"), *Filename);

	if (bStatus)
	{
		UE_LOG(LogCast, Log, TEXT("Cast Scene Loaded Succesfully"));
		CurPhase = IMPORTED;
	}
	else
	{
		ReleaseScene();
		Result = false;
		CurPhase = NOTSTARTED;
		return Result;
	}

	return Result;
}

bool FCastImporter::ImportFromFile(FString Filename)
{
	bool Result = true;

	switch (CurPhase)
	{
	case NOTSTARTED:
		if (!OpenFile(FString(Filename)))
		{
			Result = false;
			break;
		}
	case FILEOPENED:
		if (!ImportFile(FString(Filename)))
		{
			Result = false;
			CurPhase = NOTSTARTED;
			break;
		}
	case IMPORTED:
		{
			CurPhase = FIXEDANDCONVERTED;
			break;
		}
	case FIXEDANDCONVERTED:
	default:
		break;
	}

	return Result;
}

void FCastImporter::AnalysisMaterial(const FString& ParentPath, FString MaterialPath, FString TexturePath,
                                     FString TextureFormat, bool bUseGlobalTexturePath)
{
	TArray<FCastRoot>& Roots = CastManager->Scene->Roots;
	for (FCastRoot& Root : Roots)
	{
		for (FCastMaterialInfo& Material : Root.Materials)
		{
			// Parse Textures
			FString TexturesFileName = FPaths::Combine(MaterialPath, Material.Name + "_images.txt");
			if (TArray<FString> TextureContent;
				FFileHelper::LoadFileToStringArray(TextureContent, *TexturesFileName))
			{
				for (int32 LineIndex = 1; LineIndex < TextureContent.Num(); ++LineIndex)
				{
					if (bUseGlobalTexturePath)
					{
						if (FCastTextureInfo CodTexture;
							AnalysisTexture(CodTexture,
							                ParentPath,
							                TextureContent[LineIndex],
							                TexturePath,
							                TextureFormat))
						{
							Material.Textures.Add(CodTexture);
						}
					}
					else
					{
						if (FCastTextureInfo CodTexture;
							AnalysisTexture(CodTexture,
							                ParentPath,
							                TextureContent[LineIndex],
							                FPaths::Combine(TexturePath, Material.Name),
							                TextureFormat))
						{
							Material.Textures.Add(CodTexture);
						}
					}
				}
			}

			// Parse Settings
			FString SettingsFileName = FPaths::Combine(MaterialPath, Material.Name + "_settings.txt");
			if (TArray<FString> SettingsContent;
				FFileHelper::LoadFileToStringArray(SettingsContent, *SettingsFileName))
			{
				TArray<FString> TechSetLineParts;
				SettingsContent[1].ParseIntoArray(TechSetLineParts, TEXT(": "), false);
				Material.TechSet = TechSetLineParts[1];

				for (int32 LineIndex = 3; LineIndex < SettingsContent.Num(); ++LineIndex)
				{
					if (FCastSettingInfo CodSetting;
						AnalysisSetting(CodSetting,
						                SettingsContent[LineIndex]))
					{
						Material.Settings.Add(CodSetting);
					}
				}
			}
		}
	}
}

bool FCastImporter::AnalysisTexture(FCastTextureInfo& Texture, FString ParentPath, FString TextureLineText,
                                    FString TexturePath, const FString& ImageFormat)
{
	//if (ImportOptions->MaterialType == CastMT_IW9)
	//{
	//	TArray<FString> LineParts;
	//	TextureLineText.ParseIntoArray(LineParts, TEXT(","), false);
	//	FString TextureAddress = LineParts[0];
	//	FString TextureName = LineParts[1];
	//	Texture.TexturePath = FPaths::Combine(TexturePath, TextureName + "." + ImageFormat);
	//	Texture.TextureName = TextureName;
	//	Texture.TextureType = TEXT("Normal");

	//	if (TextureName[0] == '$' || !FPaths::FileExists(Texture.TexturePath))
	//	{
	//		return false;
	//	}
	//	if (TextureAddress == "unk_semantic_0x0" || TextureAddress == "unk_semantic_0x55")
	//	{
	//		Texture.TextureType = "Albedo";
	//		return ImportTexture(Texture, Texture.TexturePath, ParentPath, true);
	//	}
	//	if (TextureAddress == "unk_semantic_0x4" || TextureAddress == "unk_semantic_0x56")
	//	{
	//		Texture.TextureType = "NOG";
	//		return ImportTexture(Texture, Texture.TexturePath, ParentPath, false);
	//	}
	//	if (TextureAddress == "unk_semantic_0x8")
	//	{
	//		Texture.TextureType = "Emission";
	//		return ImportTexture(Texture, Texture.TexturePath, ParentPath, true);
	//	}
	//	if (TextureAddress == "unk_semantic_0xC")
	//	{
	//		Texture.TextureType = "Alpha";
	//		Texture.TextureType = "Alpha";
	//		return ImportTexture(Texture, Texture.TexturePath, ParentPath, false);
	//	}
	//	if (TextureAddress == "unk_semantic_0x22")
	//	{
	//		Texture.TextureType = "Transparency";
	//		return ImportTexture(Texture, Texture.TexturePath, ParentPath, true);
	//	}
	//	if (TextureAddress == "unk_semantic_0x26")
	//	{
	//		// TODO：SSS材质
	//		Texture.TextureType = "Specular_Mask";
	//		return ImportTexture(Texture, Texture.TexturePath, ParentPath, false);
	//	}
	//	if (TextureAddress == "unk_semantic_0x32")
	//	{
	//		Texture.TextureType = "Mask";
	//		return ImportTexture(Texture, Texture.TexturePath, ParentPath, true);
	//	}
	//}
	//else if (ImportOptions->MaterialType == CastMT_IW8)
	//{
	//	TArray<FString> LineParts;
	//	TextureLineText.ParseIntoArray(LineParts, TEXT(","), false);
	//	FString TextureName = LineParts[1];
	//	Texture.TexturePath = FPaths::Combine(TexturePath, TextureName + "." + ImageFormat);
	//	Texture.TextureName = TextureName;
	//	Texture.TextureType = LineParts[0];

	//	return ImportTexture(Texture, Texture.TexturePath, ParentPath, false);
	//}
	//else if (ImportOptions->MaterialType == CastMT_T7)
	//{
	//	TArray<FString> LineParts;
	//	TextureLineText.ParseIntoArray(LineParts, TEXT(","), false);
	//	FString TextureName = LineParts[1];
	//	Texture.TexturePath = FPaths::Combine(TexturePath, TextureName + "." + ImageFormat);
	//	Texture.TextureName = TextureName;
	//	Texture.TextureType = LineParts[0];

	//	return ImportTexture(Texture, Texture.TexturePath, ParentPath, true);
	//}
	//return false;
	TArray<FString> LineParts;
	TextureLineText.ParseIntoArray(LineParts, TEXT(","), false);
	FString TextureName = LineParts[1];
	Texture.TexturePath = FPaths::Combine(TexturePath, TextureName + "." + ImageFormat);
	Texture.TextureName = TextureName;
	Texture.TextureType = LineParts[0];

	return ImportTexture(Texture, Texture.TexturePath, ParentPath, true);
}

bool FCastImporter::AnalysisSetting(FCastSettingInfo& Setting, FString SettingLineText)
{
	TArray<FString> LineParts;
	SettingLineText.ParseIntoArray(LineParts, TEXT(","), false);

	// Setting Name
	Setting.Name = LineParts[1];

	// Setting Type Map
	static const TMap<FString, ESettingType> TypeMap = {
		{TEXT("Color"), ESettingType::Color},
		{TEXT("Float4"), ESettingType::Float4},
		{TEXT("Float3"), ESettingType::Float3},
		{TEXT("Float2"), ESettingType::Float2},
		{TEXT("Float1"), ESettingType::Float1}
	};

	const ESettingType* SettingType = TypeMap.Find(LineParts[0]);
	Setting.Type = *SettingType;

	switch (Setting.Type)
	{
	case Color:
	case Float4:

		Setting.Value = FVector4(
			FCString::Atof(*LineParts[2]),
			FCString::Atof(*LineParts[3]),
			FCString::Atof(*LineParts[4]),
			FCString::Atof(*LineParts[5])
		);
		break;

	case Float3:

		Setting.Value = FVector4(
			FCString::Atof(*LineParts[2]),
			FCString::Atof(*LineParts[3]),
			FCString::Atof(*LineParts[4]),
			0
		);
		break;

	case Float2:

		Setting.Value = FVector4(
			FCString::Atof(*LineParts[2]),
			FCString::Atof(*LineParts[3]),
			0,
			0
		);
		break;

	case Float1:

		Setting.Value = FVector4(
			FCString::Atof(*LineParts[2]),
			0,
			0,
			0
		);
		break;

	default:
		return false;
	}

	return true;
}

bool FCastImporter::ImportTexture(FCastTextureInfo& Texture, const FString& FilePath, const FString& ParentPath,
                                  bool bSRGB)
{
	FString TexturePath = FPaths::Combine(*ParentPath, TEXT("Materials"), TEXT("Textures"),
	                                      FPaths::GetCleanFilename(FPaths::GetPath(FilePath)));

	FString DestinationTexturePath = FPaths::Combine(TexturePath, NoIllegalSigns(FPaths::GetBaseFilename(FilePath)));

	FString AssetPath = DestinationTexturePath;
	if (UTexture2D* LoadedTexture = LoadObject<UTexture2D>(nullptr, *AssetPath))
	{
		Texture.TextureObject = LoadedTexture;
		return true;
	}

	// Create the texture factory
	UTextureFactory* TextureFactory = NewObject<UTextureFactory>();
	TextureFactory->SuppressImportOverwriteDialog();

	bool bOutOperationCanceled = false;
	// Create the parent package
	UPackage* Package = CreatePackage(*AssetPath);
	UTexture2D* ImportedTexture = (UTexture2D*)TextureFactory->FactoryCreateFile(
		UTexture2D::StaticClass(),
		Package,
		FName(NoIllegalSigns(*FPaths::GetBaseFilename(FilePath))),
		RF_Standalone | RF_Public,
		FilePath,
		nullptr, // Parms
		GWarn,
		bOutOperationCanceled
	);

	if (ImportedTexture)
	{
		if (ImportedTexture->GetSizeX() < 2 && ImportedTexture->GetSizeY() < 2) return false;

		ImportedTexture->PreEditChange(nullptr);
		ImportedTexture->GetPackage()->FullyLoad();
		ImportedTexture->GetPackage()->Modify();

		// Apply settings based on TextureType
		if (Texture.TextureType == "normalMap" ||
			Texture.TextureType == "normalBodyMap" ||
			Texture.TextureType == "detailMap" ||
			Texture.TextureType == "detailMap1" ||
			Texture.TextureType == "detailMap2" ||
			Texture.TextureType == "detailNormal1" ||
			Texture.TextureType == "detailNormal2" ||
			Texture.TextureType == "detailNormal3" ||
			Texture.TextureType == "detailNormal4" ||
			Texture.TextureType == "transitionNormal" ||
			Texture.TextureType == "flagRippleDetailMap" ||
			Texture.TextureType == "distortionMap" ||
			Texture.TextureType == "crackNormalMap")
		{
			ImportedTexture->CompressionSettings = TC_Normalmap;
			ImportedTexture->bFlipGreenChannel = false;
		}

		else if (Texture.TextureType == "aoMap" ||
			Texture.TextureType == "alphaMaskMap" ||
			Texture.TextureType == "alphaMask" ||
			Texture.TextureType == "breakUpMap" ||
			Texture.TextureType == "specularMask" ||
			Texture.TextureType == "specularMaskDetail2" ||
			Texture.TextureType == "tintMask" ||
			Texture.TextureType == "tintBlendMask" ||
			Texture.TextureType == "thicknessMap" ||
			Texture.TextureType == "revealMap" ||
			Texture.TextureType == "transRevealMap" ||
			Texture.TextureType == "thermalHeatmap" ||
			Texture.TextureType == "transGlossMap" ||
			Texture.TextureType == "flickerLookupMap" ||
			Texture.TextureType == "glossMap" ||
			Texture.TextureType == "glossBodyMap" ||
			Texture.TextureType == "glossMapDetail2")
		{
			ImportedTexture->CompressionSettings = TC_Grayscale;
			ImportedTexture->SRGB = false;
		}

		else if (Texture.TextureType == "customizeMask" ||
			Texture.TextureType == "flowMap" ||
			Texture.TextureType == "mixMap" ||
			Texture.TextureType == "camoMaskMap" ||
			Texture.TextureType == "detailNormalMask")
		{
			ImportedTexture->CompressionSettings = TC_Masks;
		}
		// IW8
		else if (Texture.TextureType == "unk_semantic_0x9" ||
			Texture.TextureType == "unk_semantic_0xA" ||
			// IW9/JUP
			Texture.TextureType == "unk_semantic_0x4" ||
			Texture.TextureType == "unk_semantic_0x5" ||
			// T10
			Texture.TextureType == "unk_semantic_0x58" ||
			Texture.TextureType == "unk_semantic_0x65")
		{
			ImportedTexture->CompressionSettings = TC_Default;
			ImportedTexture->SRGB = false;
		}

		ImportedTexture->PostEditChange();
		ImportedTexture->UpdateResource();
		ImportedTexture->MarkPackageDirty();
		UEditorLoadingAndSavingUtils::SavePackages(TArray<UPackage*>{ImportedTexture->GetPackage()}, false);
		Texture.TextureObject = ImportedTexture;
		return true;
	}

	return false;
}

UMaterialInterface* FCastImporter::CreateMaterialInstance(const FCastMaterialInfo& Material,
                                                          const UObject* ParentPackage)
{
	UMaterialInterface* UnrealMaterialFinal = nullptr;
	FString FullMaterialName = Material.Name;
	FString MaterialType = TEXT("Base");
	bool isMetallic = false;
	bool isHead = false;
	const auto MaterialInstanceFactory = NewObject<UMaterialInstanceConstantFactoryNew>();

	FString MaterialPath;
	// if (ImportOptions->MaterialType == ECastMaterialType::CastMT_T7)
	// {
	// MaterialPath = FPaths::Combine("/UGC4579750/black_ops_2/Shading/T7/TechSets", Material.TechSet);
	// }
	if (ImportOptions->MaterialType == ECastMaterialType::CastMT_IW8)
	{
		for (const auto Texture : Material.Textures)
		{
			// Check if metallic
			if (Texture.TextureType == TEXT("unk_semantic_0x0"))
			{
				if (Texture.TextureObject->HasAlphaChannel())
				{
					isMetallic = true;
				}
			}
			// Check if Skin
			if (Texture.TextureType == TEXT("unk_semantic_0x4D"))
			{
				MaterialType = TEXT("Skin");
				// Check if Head
				//if (Texture.TextureType == TEXT("unk_semantic_0x7F"))
				//	isHead = true;
			}
			// Check if Hair
			if (Texture.TextureType == TEXT("unk_semantic_0x85"))
			{
				MaterialType = TEXT("Hair");
			}
			// Check if Eye
			if (Texture.TextureType == TEXT("unk_semantic_0x86"))
			{
				MaterialType = TEXT("Eye");
			}
		}
		MaterialPath = FPaths::Combine("/IWToUE/Shading/IW/IW8", "IW8_" + MaterialType);
	}
	if (ImportOptions->MaterialType == ECastMaterialType::CastMT_IW9)
	{
		for (const auto Texture : Material.Textures)
		{
			// Check if metallic
			if (Texture.TextureType == TEXT("unk_semantic_0x0"))
			{
				if (Texture.TextureObject->HasAlphaChannel())
				{
					isMetallic = true;
				}
			}
			//// Check if Skin
			// if (Texture.TextureType == TEXT("unk_semantic_0x26"))
			// {
			// 	 if (Texture.TextureName != "$black" && Texture.TextureName != "$white")
			// 	 	MaterialType = TEXT("Skin");
			// 	//Check if Head
			// 	if (Texture.TextureType == TEXT("unk_semantic_0x7F"))
			// 		isHead = true;
			// }
			//// Check if Hair
			// if (Texture.TextureType == TEXT("unk_semantic_0x85"))
			// {
			// 	MaterialType = TEXT("Hair");
			// }
			//// Check if Eye
			// if (Texture.TextureType == TEXT("unk_semantic_0x86"))
			// {
			// 	MaterialType = TEXT("Eye");
			// }
		}
		MaterialPath = FPaths::Combine("/IWToUE/Shading/IW/IW9", "IW9_" + MaterialType);
	}
	if (ImportOptions->MaterialType == ECastMaterialType::CastMT_T10)
	{
		for (const auto Texture : Material.Textures)
		{
			// Check if metallic
			if (Texture.TextureType == TEXT("unk_semantic_0x57"))
			{
				if (Texture.TextureObject->HasAlphaChannel())
				{
					isMetallic = true;
				}
			}
			// // Check if Skin
			// if (Texture.TextureType == TEXT("unk_semantic_0x4D"))
			// {
			// 	MaterialType = TEXT("Skin");
			// 	// Check if Head
			// 	if (Texture.TextureType == TEXT("unk_semantic_0x7F"))
			// 		isHead = true;
			// }
			// // Check if Hair
			// if (Texture.TextureType == TEXT("unk_semantic_0x85"))
			// {
			// 	MaterialType = TEXT("Hair");
			// }
			// // Check if Eye
			// if (Texture.TextureType == TEXT("unk_semantic_0x86"))
			// {
			// 	MaterialType = TEXT("Eye");
			// }
		}
		MaterialPath = FPaths::Combine("/IWToUE/Shading/IW/T10", "T10_" + MaterialType);
	}

	MaterialInstanceFactory->InitialParent =
		Cast<UMaterial>(StaticLoadObject(UMaterial::StaticClass(), nullptr, *MaterialPath));
	const auto MaterialPackage = CreatePackage(
		*FPaths::Combine(FPaths::GetPath(ParentPackage->GetPathName()),TEXT("Materials/"), FullMaterialName));
	check(MaterialPackage);

	MaterialPackage->FullyLoad();
	MaterialPackage->Modify();
	UObject* MaterialInstObject = MaterialInstanceFactory->FactoryCreateNew(
		UMaterialInstanceConstant::StaticClass(), MaterialPackage, *FullMaterialName, RF_Standalone | RF_Public, NULL,
		GWarn);
	UMaterialInstanceConstant* MaterialInstance = Cast<UMaterialInstanceConstant>(MaterialInstObject);
	FAssetRegistryModule::AssetCreated(MaterialInstance);

	FStaticParameterSet StaticParameters;
	MaterialInstance->GetStaticParameterValues(StaticParameters);

	for (auto Texture : Material.Textures)
	{
		if (!Texture.TextureObject) { continue; }
		UTexture* TextureAsset = Cast<UTexture>(Texture.TextureObject);
		if (TextureAsset && !Texture.TextureType.IsEmpty())
		{
			FMaterialParameterInfo TextureParameterInfo(*Texture.TextureType, GlobalParameter, -1);
			MaterialInstance->SetTextureParameterValueEditorOnly(TextureParameterInfo, TextureAsset);
		}
	}

	for (auto Setting : Material.Settings)
	{
		// Get the right parameter
		FMaterialParameterInfo ParameterInfo(*Setting.Name, GlobalParameter, -1);

		switch (Setting.Type)
		{
		case Float1:

			MaterialInstance->SetScalarParameterValueEditorOnly(ParameterInfo, Setting.Value.X);
			break;

		case Float2:

			MaterialInstance->SetVectorParameterValueEditorOnly(ParameterInfo,
			                                                    FLinearColor(Setting.Value.X, Setting.Value.Y, 0, 0));
			break;

		case Float3:

			MaterialInstance->SetVectorParameterValueEditorOnly(ParameterInfo,
			                                                    FLinearColor(
				                                                    Setting.Value.X, Setting.Value.Y, Setting.Value.Z,
				                                                    0));
			break;

		case Color:
		case Float4:

			MaterialInstance->SetVectorParameterValueEditorOnly(ParameterInfo,
			                                                    FLinearColor(
				                                                    Setting.Value.X, Setting.Value.Y, Setting.Value.Z,
				                                                    Setting.Value.W));
			break;
		}
	}

	if (isMetallic)
	{
		FMaterialParameterInfo ParameterInfo("Default Spec", GlobalParameter, -1);
		MaterialInstance->SetScalarParameterValueEditorOnly(ParameterInfo, 0);
	}
	//if (isHead)
	//{
	//	FMaterialParameterInfo ParameterInfo("Roughness Multiplier", GlobalParameter, -1);
	//	MaterialInstance->SetScalarParameterValueEditorOnly(ParameterInfo, 2);
	//}

	MaterialInstance->UpdateStaticPermutation(StaticParameters);

	UnrealMaterialFinal = MaterialInstance;
	UnrealMaterialFinal->PreEditChange(nullptr);
	UnrealMaterialFinal->PostEditChange();
	UnrealMaterialFinal->GetPackage()->FullyLoad();
	UnrealMaterialFinal->MarkPackageDirty();

	return UnrealMaterialFinal;
}

void FCastImporter::ClearAllCaches()
{
}

FCastImporter* FCastImporter::GetInstance(bool bDoNotCreate)
{
	if (!StaticInstance.IsValid())
	{
		//Return nullptr if we cannot create the instance
		if (bDoNotCreate)
		{
			return nullptr;
		}

		StaticInstance = MakeShareable(new FCastImporter());
	}
	return StaticInstance.Get();
}

void FCastImporter::DeleteInstance()
{
	StaticInstance.Reset();
}

FString FCastImporter::MakeName(const FString& Name)
{
	const TCHAR SpecialChars[] = {TEXT('.'), TEXT(','), TEXT('/'), TEXT('`'), TEXT('%')};

	FString TmpName = Name;

	int32 LastNamespaceTokenIndex = INDEX_NONE;
	if (TmpName.FindLastChar(TEXT(':'), LastNamespaceTokenIndex))
	{
		TmpName.RightChopInline(LastNamespaceTokenIndex + 1, EAllowShrinking::Yes);
	}

	for (int32 i = 0; i < UE_ARRAY_COUNT(SpecialChars); i++)
	{
		TmpName.ReplaceCharInline(SpecialChars[i], TEXT('_'), ESearchCase::CaseSensitive);
	}

	return TmpName;
}

FCastImportOptions* FCastImporter::GetImportOptions() const
{
	return ImportOptions.Get();
}

FCastImportOptions* FCastImporter::GetImportOptions(
	UCastImportUI* ImportUI,
	bool bShowOptionDialog,
	bool bIsAutomated,
	const FString& FullPath,
	bool& OutOperationCanceled,
	bool& bOutImportAll,
	const FString& InFilename)
{
	OutOperationCanceled = false;

	if (bShowOptionDialog && !bIsAutomated)
	{
		bOutImportAll = false;

		TSharedPtr<SWindow> ParentWindow;

		if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
		{
			IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
			ParentWindow = MainFrame.GetParentWindow();
		}

		const float ImportWindowWidth = 450.0f;
		const float ImportWindowHeight = 750.0f;
		FVector2D ImportWindowSize = FVector2D(ImportWindowWidth, ImportWindowHeight);

		FSlateRect WorkAreaRect = FSlateApplicationBase::Get().GetPreferredWorkArea();
		FVector2D DisplayTopLeft(WorkAreaRect.Left, WorkAreaRect.Top);
		FVector2D DisplaySize(WorkAreaRect.Right - WorkAreaRect.Left, WorkAreaRect.Bottom - WorkAreaRect.Top);

		float ScaleFactor = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(DisplayTopLeft.X, DisplayTopLeft.Y);
		ImportWindowSize *= ScaleFactor;

		FVector2D WindowPosition = (DisplayTopLeft + (DisplaySize - ImportWindowSize) / 2.0f) / ScaleFactor;

		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(NSLOCTEXT("UnrealEd", "CastImportOpionsTitle", "Cast Import Options"))
			.SizingRule(ESizingRule::Autosized)
			.AutoCenter(EAutoCenter::None)
			.ClientSize(ImportWindowSize)
			.ScreenPosition(WindowPosition);

		TSharedPtr<SCastOptionWindow> CastOptionWindow;
		Window->SetContent
		(
			SAssignNew(CastOptionWindow, SCastOptionWindow)
			.ImportUI(ImportUI)
			.WidgetWindow(Window)
			.FullPath(FText::FromString(FullPath))
			.MaxWindowHeight(ImportWindowHeight)
			.MaxWindowWidth(ImportWindowWidth)
		);
		FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);

		ImportOptions->PhysicsAsset = ImportUI->PhysicsAsset;
		ImportOptions->Skeleton = ImportUI->Skeleton;
		ImportOptions->bImportMaterial = ImportUI->bImportMaterial;
		ImportOptions->TexturePathType = ImportUI->TexturePathType;
		ImportOptions->GlobalMaterialPath = ImportUI->GlobalMaterialPath;
		ImportOptions->TextureFormat = ImportUI->TextureFormat;
		ImportOptions->bImportAsSkeletal = ImportUI->bImportAsSkeletal;
		ImportOptions->bImportMesh = ImportUI->bImportMesh;
		ImportOptions->bImportAnimations = ImportUI->bImportAnimations;
		ImportOptions->bConvertRefPosition = ImportUI->bConvertRefPosition;
		ImportOptions->bConvertRefAnim = ImportUI->bConvertRefAnim;
		ImportOptions->bReverseFace = ImportUI->bReverseFace;
		ImportOptions->bImportAnimationNotify = ImportUI->bImportAnimationNotify;
		ImportOptions->bDeleteRootNodeAnim = ImportUI->bDeleteRootNodeAnim;
		ImportOptions->MaterialType = ImportUI->MaterialType;

		if (CastOptionWindow->ShouldImport())
		{
			bOutImportAll = CastOptionWindow->ShouldImportAll();
			return ImportOptions.Get();
		}
		OutOperationCanceled = true;
	}
	return ImportOptions.Get();
}

USkeletalMesh* FCastImporter::ImportSkeletalMesh(CastScene::FImportSkeletalMeshArgs& ImportSkeletalMeshArgs)
{
	if (!CastManager || !CastManager->Scene || CastManager->Scene->Roots.IsEmpty() ||
		CastManager->Scene->Roots[0].ModelLodInfo.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("ImportSkeletalMesh: Invalid scene data provided."));
		return nullptr;
	}

	FCastRoot& SceneRoot = CastManager->Scene->Roots[0];
	const int32 LodCount = SceneRoot.ModelLodInfo.Num();
	if (LodCount == 0 || SceneRoot.Models.Num() != LodCount)
	{
		UE_LOG(LogTemp, Error, TEXT("ImportSkeletalMesh: Mismatched LOD count (%d) and Model count (%d)."), LodCount,
		       SceneRoot.Models.Num());
		return nullptr;
	}

	FScopedSlowTask SlowTask(LodCount + 2,
	                         FText::Format(
		                         NSLOCTEXT("CastImporter", "ImportingSkeletalMesh", "Importing Skeletal Mesh {0}"),
		                         FText::FromName(ImportSkeletalMeshArgs.Name)));
	SlowTask.MakeDialog();
	USkeletalMesh* SkeletalMesh = NewObject<USkeletalMesh>(ImportSkeletalMeshArgs.InParent,
	                                                       ImportSkeletalMeshArgs.Name,
	                                                       ImportSkeletalMeshArgs.Flags);
	CreatedObjects.Add(SkeletalMesh);
	SkeletalMesh->PreEditChange(nullptr);

	// --- 材质处理 ---
	SlowTask.EnterProgressFrame(1, NSLOCTEXT("CastImporter", "ProcessingMaterials", "Processing Materials"));
	TArray<SkeletalMeshImportData::FMaterial> GlobalDataMaterials;
	TArray<FSkeletalMaterial>& SkeletalMeshMaterials = SkeletalMesh->GetMaterials();
	SkeletalMeshMaterials.Empty(SceneRoot.Materials.Num());
	TMap<uint32, int32> GlobalMaterialHashMapToIndexMap;

	CreatedObjects.Add(SkeletalMesh);

	FSkeletalMeshImportData Data;
	{
		// 处理骨骼
		TArray<int32> ChildrenCount;
		ChildrenCount.SetNumZeroed(CastManager->GetBoneNum());
		for (FCastRoot& Root : CastManager->Scene->Roots)
		{
			for (FCastModelInfo& Model : Root.Models)
			{
				for (FCastSkeletonInfo& Skeleton : Model.Skeletons)
				{
					for (FCastBoneInfo& Bone : Skeleton.Bones)
					{
						if (Bone.ParentIndex != -1)
						{
							ChildrenCount[Bone.ParentIndex]++;
						}
					}
				}
			}
		}
		for (FCastRoot& Root : CastManager->Scene->Roots)
		{
			for (FCastModelInfo& Model : Root.Models)
			{
				for (FCastSkeletonInfo& Skeleton : Model.Skeletons)
				{
					for (int32 i = 0; i < Skeleton.Bones.Num(); ++i)
					{
						FCastBoneInfo& Bone = Skeleton.Bones[i];
						SkeletalMeshImportData::FBone BoneBinary;
						BoneBinary.Name = Bone.BoneName;
						BoneBinary.ParentIndex = Bone.ParentIndex;
						BoneBinary.NumChildren = ChildrenCount[i];

						SkeletalMeshImportData::FJointPos& JointMatrix = BoneBinary.BonePos;
						JointMatrix.Length = 1.;
						JointMatrix.XSize = JointMatrix.YSize = JointMatrix.ZSize = 100.f;

						JointMatrix.Transform.SetTranslation(FVector3f{
							Bone.LocalPosition.X, -Bone.LocalPosition.Y, Bone.LocalPosition.Z
						});

						FQuat4f RawBoneRotator(Bone.LocalRotation.X, -Bone.LocalRotation.Y, Bone.LocalRotation.Z,
						                       -Bone.LocalRotation.W);
						JointMatrix.Transform.SetRotation(RawBoneRotator);

						JointMatrix.Transform.SetScale3D(Bone.Scale);

						Data.RefBonesBinary.Add(BoneBinary);
					}
				}
			}
		}
		// 顶点
		Data.Points.Empty();
		for (FCastRoot& Root : CastManager->Scene->Roots)
		{
			for (FCastModelInfo& Model : Root.Models)
			{
				for (FCastMeshInfo& Mesh : Model.Meshes)
				{
					for (int32 i = 0; i < Mesh.VertexPositions.Num(); ++i)
					{
						Data.Points.Add(FVector3f(Mesh.VertexPositions[i].X,
						                          -Mesh.VertexPositions[i].Y,
						                          Mesh.VertexPositions[i].Z));
					}
				}
			}
		}

		// 一般是两个UV层，第一个是正常UV，第二个是光照贴图
		// 可以支持更多UV层，暂时还没发现这样的cast文件
		Data.NumTexCoords = 2;
		// 面 和 材质
		int32 VertexOffset = 0;
		TArray<int32> VertexOrder = {0, 1, 2};
		if (ImportOptions->bReverseFace)
		{
			VertexOrder = {2, 1, 0};
		}
		TMap<uint32, int32> DataMatMap;
		for (FCastRoot& Root : CastManager->Scene->Roots)
		{
			for (FCastModelInfo& Model : Root.Models)
			{
				for (FCastMeshInfo& Mesh : Model.Meshes)
				{
					uint32 ModelMatIdx = *Root.MaterialMap.Find(Mesh.MaterialHash);
					int32 MatIdx;
					if (int32* FindRes = DataMatMap.Find(ModelMatIdx))
					{
						MatIdx = *FindRes;
					}
					else
					{
						const FCastMaterialInfo& Material = Root.Materials[ModelMatIdx];
						SkeletalMeshImportData::FMaterial NewMaterial;
						NewMaterial.MaterialImportName = Material.Name;
						if (Material.Textures.Num() > 0)
						{
							NewMaterial.Material =
								CreateMaterialInstance(Material, ImportSkeletalMeshArgs.InParent);
						}
						MatIdx = Data.Materials.Add(NewMaterial);
						DataMatMap.Add(ModelMatIdx, MatIdx);
					}

					const int32 MeshFaceCnt = Mesh.Faces.Num() / 3;
					for (int32 FaceID = 0; FaceID < MeshFaceCnt; ++FaceID)
					{
						SkeletalMeshImportData::FTriangle& Triangle = Data.Faces.AddZeroed_GetRef();
						Triangle.SmoothingGroups = 255;
						uint32 TriangleVertexID[3] = {
							Mesh.Faces[FaceID * 3], Mesh.Faces[FaceID * 3 + 1], Mesh.Faces[FaceID * 3 + 2]
						};
						for (int32 FaceVertexID = 0; FaceVertexID < 3; ++FaceVertexID)
						{
							int32 WedgesID = Data.Wedges.AddUninitialized();
							SkeletalMeshImportData::FVertex& Wedges = Data.Wedges[WedgesID];

							const int32 MeshVertexID = TriangleVertexID[VertexOrder[FaceVertexID]];
							Wedges.MatIndex = MatIdx;
							Wedges.VertexIndex = MeshVertexID + VertexOffset;
							if (Mesh.VertexColor.IsValidIndex(MeshVertexID))
							{
								Wedges.Color = FColor((Mesh.VertexColor[MeshVertexID] >> 0) & 0xFF,
								                      (Mesh.VertexColor[MeshVertexID] >> 8) & 0xFF,
								                      (Mesh.VertexColor[MeshVertexID] >> 16) & 0xFF,
								                      (Mesh.VertexColor[MeshVertexID] >> 24) & 0xFF);
							}
							Wedges.UVs[0] = FVector2f(Mesh.VertexUV[MeshVertexID].X, Mesh.VertexUV[MeshVertexID].Y);
							Wedges.Reserved = 0;

							Triangle.TangentZ[FaceVertexID] = FVector3f{
								Mesh.VertexNormals[MeshVertexID].X,
								-Mesh.VertexNormals[MeshVertexID].Y,
								Mesh.VertexNormals[MeshVertexID].Z
							};
							Triangle.TangentZ->Normalize();
							Triangle.WedgeIndex[FaceVertexID] = WedgesID;
						}
						Triangle.MatIndex = MatIdx;
					}
					VertexOffset += Mesh.VertexPositions.Num();
				}
			}
		}

		// 权重
		VertexOffset = 0;
		int32 BoneOffset = 0;
		for (FCastRoot& Root : CastManager->Scene->Roots)
		{
			for (FCastModelInfo& Model : Root.Models)
			{
				for (FCastMeshInfo& Mesh : Model.Meshes)
				{
					for (uint32 WightID = 0; WightID < (uint32)Mesh.VertexWeightBone.Num(); ++WightID)
					{
						Data.Influences.AddUninitialized();
						Data.Influences.Last().VertexIndex = (int32)(WightID / Mesh.MaxWeight) + VertexOffset;
						Data.Influences.Last().BoneIndex = Mesh.VertexWeightBone[WightID] + BoneOffset;
						Data.Influences.Last().Weight = Mesh.VertexWeightValue[WightID];
					}
					VertexOffset += Mesh.VertexPositions.Num();
				}
				for (FCastSkeletonInfo& Skeleton : Model.Skeletons)
				{
					BoneOffset += Skeleton.Bones.Num();
				}
			}
		}
	}

	FSkeletalMeshBuildSettings BuildOptions;

	BuildOptions.bUseFullPrecisionUVs = false;
	BuildOptions.bUseBackwardsCompatibleF16TruncUVs = false;
	BuildOptions.bUseHighPrecisionTangentBasis = false;
	BuildOptions.bRecomputeNormals = false;
	BuildOptions.bRecomputeTangents = false;
	BuildOptions.bUseMikkTSpace = false;
	BuildOptions.bComputeWeightedNormals = false;
	BuildOptions.bRemoveDegenerates = false;
	BuildOptions.ThresholdPosition = false;
	BuildOptions.ThresholdTangentNormal = false;
	BuildOptions.ThresholdUV = false;
	BuildOptions.MorphThresholdPosition = false;

	SkeletalMesh->PreEditChange(nullptr);
	SkeletalMesh->InvalidateDeriveDataCacheGUID();

	FSkeletalMeshModel* ImportedResource = SkeletalMesh->GetImportedModel();
	ImportedResource->LODModels.Empty();
	ImportedResource->LODModels.Add(new FSkeletalMeshLODModel());
	FSkeletalMeshLODModel& LODModel = SkeletalMesh->GetImportedModel()->LODModels[0];
	LODModel.NumTexCoords = 2;

	// 处理材质
	TArray<FSkeletalMaterial>& InMaterials = SkeletalMesh->GetMaterials();
	InMaterials.Empty();
	for (SkeletalMeshImportData::FMaterial& Material : Data.Materials)
	{
		InMaterials.Add(FSkeletalMaterial(Material.Material.Get(), true, false,
		                                  *Material.MaterialImportName,
		                                  *Material.MaterialImportName));
	}
	// 处理骨架，FReferenceSkeletonModifier调用析构函数时确认修改骨架
	{
		FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();
		RefSkeleton.Empty();
		FReferenceSkeletonModifier RefSkelModifier(RefSkeleton, nullptr);
		for (SkeletalMeshImportData::FBone& Bone : Data.RefBonesBinary)
		{
			FString BoneName = Bone.Name;
			FMeshBoneInfo BoneInfo(FName(*BoneName, FNAME_Add), BoneName, Bone.ParentIndex);
			FTransform BoneTransform(Bone.BonePos.Transform);
			RefSkelModifier.Add(BoneInfo, BoneTransform);
		}
	}

	SkeletalMesh->ResetLODInfo();
	FSkeletalMeshLODInfo& NewLODInfo = SkeletalMesh->AddLODInfo();
	NewLODInfo.ReductionSettings.NumOfTrianglesPercentage = 1.0f;
	NewLODInfo.ReductionSettings.NumOfVertPercentage = 1.0f;
	NewLODInfo.ReductionSettings.MaxDeviationPercentage = 0.0f;
	NewLODInfo.LODHysteresis = 0.02f;

	Data.bHasNormals = true;

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	SkeletalMesh->SaveLODImportedData(0, Data);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// 边界框
	TArray<FVector> Points{FVector::ZeroVector};
	bool bHasVertexColors = false;
	for (FCastRoot& Root : CastManager->Scene->Roots)
	{
		for (FCastModelInfo& Model : Root.Models)
		{
			for (FCastMeshInfo& Mesh : Model.Meshes)
			{
				Mesh.ComputeBBox();
				Points.Add(Mesh.BBoxMax);
				Points.Add(Mesh.BBoxMin);
				bHasVertexColors = Mesh.VertexColor.Num() > 0;
			}
		}
	}

	SkeletalMesh->SetImportedBounds(FBoxSphereBounds(FBox(Points)));
	SkeletalMesh->SetHasVertexColors(bHasVertexColors);
	SkeletalMesh->SetVertexColorGuid(SkeletalMesh->GetHasVertexColors() ? FGuid::NewGuid() : FGuid());

	SkeletalMesh->GetLODInfo(0)->BuildSettings = BuildOptions;
	IMeshBuilderModule& MeshBuilderModule = IMeshBuilderModule::GetForRunningPlatform();
	FSkeletalMeshBuildParameters SkeletalMeshBuildParameters(SkeletalMesh,
	                                                         GetTargetPlatformManagerRef().GetRunningTargetPlatform(),
	                                                         0, false);
	bool bBuildSuccess = MeshBuilderModule.BuildSkeletalMesh(SkeletalMeshBuildParameters);
	if (!bBuildSuccess)
	{
		SkeletalMesh->MarkAsGarbage();
		return NULL;
	}
	SkeletalMesh->GetAssetImportData()->AddFileName(UFactory::GetCurrentFilename(), 0,
	                                                NSSkeletalMeshSourceFileLabels::GeoAndSkinningText().ToString());
	SkeletalMesh->CalculateInvRefMatrices();
	if (!SkeletalMesh->GetResourceForRendering()
		|| !SkeletalMesh->GetResourceForRendering()->LODRenderData.IsValidIndex(0))
	{
		SkeletalMesh->Build();
	}
	SkeletalMesh->MarkPackageDirty();

	USkeleton* Skeleton = nullptr;
	if (!Skeleton)
	{
		FString ObjectName = FString::Printf(TEXT("%s_Skeleton"), *SkeletalMesh->GetName());
		Skeleton = CreateAsset<USkeleton>(ImportSkeletalMeshArgs.InParent->GetName(), ObjectName, true);
		if (Skeleton)
		{
			CreatedObjects.Add(Skeleton);
		}
	}
	Skeleton->MergeAllBonesToBoneTree(SkeletalMesh);
	if (SkeletalMesh->GetSkeleton() != Skeleton)
	{
		SkeletalMesh->SetSkeleton(Skeleton);
		SkeletalMesh->MarkPackageDirty();
	}
	// 创建物资资产
	if (!SkeletalMesh->GetPhysicsAsset())
	{
		FString ObjectName = FString::Printf(TEXT("%s_PhysicsAsset"), *SkeletalMesh->GetName());
		UPhysicsAsset* NewPhysicsAsset = CreateAsset<UPhysicsAsset>(ImportSkeletalMeshArgs.InParent->GetName(),
		                                                            ObjectName, true);
		FPhysAssetCreateParams NewBodyData;
		FText CreationErrorMessage;
		CreatedObjects.Add(NewPhysicsAsset);
		FPhysicsAssetUtils::CreateFromSkeletalMesh(NewPhysicsAsset, SkeletalMesh, NewBodyData,
		                                           CreationErrorMessage);
	}
	else if (ImportOptions->PhysicsAsset)
	{
		SkeletalMesh->SetPhysicsAsset(ImportOptions->PhysicsAsset);
	}

	return SkeletalMesh;
}

UStaticMesh* FCastImporter::ImportStaticMesh(UObject* InParent, const FName InName, EObjectFlags Flags)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FCastImporter::ImportStaticMesh);

	FString MeshName = ObjectTools::SanitizeObjectName(InName.ToString());
	UPackage* Package = nullptr;
	if (InParent && InParent->IsA(UPackage::StaticClass()))
	{
		Package = StaticCast<UPackage*>(InParent);
	}

	UStaticMesh* StaticMesh = NewObject<UStaticMesh>(Package, FName(*MeshName), Flags | RF_Public);
	CreatedObjects.Add(StaticMesh);

	for (FCastRoot& Root : CastManager->Scene->Roots)
	{
		for (FCastModelInfo& Model : Root.Models)
		{
			BuildStaticMeshFromModel(InParent, Root, Model, StaticMesh);
		}
	}

	return StaticMesh;
}

UAnimSequence* FCastImporter::ImportAnim(UObject* InParent, USkeleton* Skeleton)
{
	if (!Skeleton)
	{
		return nullptr;
	}
	UAnimSequence* AnimSequence = nullptr;

	for (FCastRoot& Root : CastManager->Scene->Roots)
	{
		for (FCastAnimationInfo& Animation : Root.Animations)
		{
			FString SequenceName = FPaths::GetBaseFilename(InParent->GetName());
			if (Root.Animations.Num() > 1)
			{
				SequenceName += "_";
				SequenceName += Animation.Name;
			}
			FString ParentPath = FString::Printf(TEXT("%s/%s"),
			                                     *FPackageName::GetLongPackagePath(*InParent->GetName()),
			                                     *SequenceName);
			UObject* ParentPackage = CreatePackage(*ParentPath);
			UAnimSequence* DestSeq =
				NewObject<UAnimSequence>(ParentPackage, *SequenceName, RF_Public | RF_Standalone);
			CreatedObjects.Add(DestSeq);

			DestSeq->SetSkeleton(Skeleton);
			DestSeq->ImportFileFramerate = Animation.Framerate;

			ImportAnimation(Skeleton, DestSeq, Animation);
			AnimSequence = DestSeq;
		}
	}

	return AnimSequence;
}

void FCastImporter::BuildStaticMeshFromModel(UObject* ParentPackage, FCastRoot& SceneRoot, FCastModelInfo& Model,
                                             UStaticMesh* StaticMesh)
{
	FMeshDescription MeshDescription;
	FStaticMeshAttributes Attributes(MeshDescription);
	Attributes.Register();
	Attributes.RegisterTriangleNormalAndTangentAttributes();

	TVertexAttributesRef<FVector3f> VertexPositions = Attributes.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> VertexInstanceNormals = Attributes.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector4f> VertexInstanceColors = Attributes.GetVertexInstanceColors();
	TVertexInstanceAttributesRef<FVector2f> VertexInstanceUVs = Attributes.GetVertexInstanceUVs();
	TPolygonGroupAttributesRef<FName> PolygonGroupImportedMaterialSlotNames =
		Attributes.GetPolygonGroupMaterialSlotNames();

	int32 VertexCount = 0;
	uint32 UVLayer = 0;

	for (FCastMeshInfo& Mesh : Model.Meshes)
	{
		VertexCount += Mesh.VertexPositions.Num();
		UVLayer = FMath::Max(UVLayer, Mesh.UVLayer);
	}

	VertexInstanceUVs.SetNumChannels(UVLayer + 1);
	MeshDescription.ReserveNewVertices(VertexCount);
	MeshDescription.ReserveNewVertexInstances(VertexCount);
	MeshDescription.ReserveNewPolygons(VertexCount);
	MeshDescription.ReserveNewEdges(VertexCount * 2);
	TArray<FVertexInstanceID> VertexInstanceIDs;
	VertexInstanceIDs.Reserve(VertexCount);
	TArray<uint32> VertexIdOffset;
	VertexIdOffset.Add(0);

	for (FCastMeshInfo& Mesh : Model.Meshes)
	{
		for (int32 i = 0; i < (int32)Mesh.VertexPositions.Num(); ++i)
		{
			const FVertexID VertexID = MeshDescription.CreateVertex();
			VertexPositions[VertexID] = FVector3f(Mesh.VertexPositions[i].X,
			                                      -Mesh.VertexPositions[i].Y,
			                                      Mesh.VertexPositions[i].Z);

			const FVertexInstanceID VertexInstanceID = MeshDescription.CreateVertexInstance(VertexID);
			VertexInstanceIDs.Add(VertexInstanceID);

			if (Mesh.VertexNormals.IsValidIndex(i))
			{
				VertexInstanceNormals[VertexInstanceID] = FVector3f(Mesh.VertexNormals[i].X,
				                                                    -Mesh.VertexNormals[i].Y,
				                                                    Mesh.VertexNormals[i].Z);
			}
			if (Mesh.VertexColor.IsValidIndex(i))
			{
				VertexInstanceColors[VertexInstanceID] = FVector4f((Mesh.VertexColor[i] >> 0) & 0xFF,
				                                                   (Mesh.VertexColor[i] >> 8) & 0xFF,
				                                                   (Mesh.VertexColor[i] >> 16) & 0xFF,
				                                                   (Mesh.VertexColor[i] >> 24) & 0xFF);
			}
			if (Mesh.VertexUV.IsValidIndex(i))
				VertexInstanceUVs.Set(VertexInstanceID, 0, FVector2f(Mesh.VertexUV[i].X, Mesh.VertexUV[i].Y));
		}
		VertexIdOffset.Add(MeshDescription.Vertices().Num());
	}

	for (uint32 k = 0; k < (uint32)Model.Meshes.Num(); ++k)
	{
		const FCastMeshInfo& Mesh = Model.Meshes[k];
		const FPolygonGroupID PolygonGroup = MeshDescription.CreatePolygonGroup();

		for (int32 i = 0; i < (int32)Mesh.Faces.Num(); i += 3)
		{
			TArray<FVertexInstanceID> TriangleVertexInstanceIDs;
			for (int32 j = 0; j < 3; j++)
			{
				uint32 VertexIndex = Mesh.Faces[i + j] + VertexIdOffset[k];
				TriangleVertexInstanceIDs.Add(VertexInstanceIDs[VertexIndex]);
			}
			MeshDescription.CreatePolygon(PolygonGroup, TriangleVertexInstanceIDs);
		}

		uint32 MatIdx = *SceneRoot.MaterialMap.Find(Mesh.MaterialHash);
		const FCastMaterialInfo& Material = SceneRoot.Materials[MatIdx];
		FName MatName = FName(*Material.Name);
		PolygonGroupImportedMaterialSlotNames[PolygonGroup] = MatName;
	}

	StaticMesh->InitResources();
	StaticMesh->SetLightingGuid();
	FStaticMeshSourceModel& SrcModel = StaticMesh->AddSourceModel();
	SrcModel.BuildSettings.bRecomputeNormals = false;
	SrcModel.BuildSettings.bRecomputeTangents = true;
	SrcModel.BuildSettings.bRemoveDegenerates = false;
	SrcModel.BuildSettings.bUseHighPrecisionTangentBasis = false;
	SrcModel.BuildSettings.bUseFullPrecisionUVs = false;
	SrcModel.BuildSettings.bGenerateLightmapUVs = true;
	SrcModel.BuildSettings.SrcLightmapIndex = 0;
	SrcModel.BuildSettings.DstLightmapIndex = UVLayer;
	SrcModel.BuildSettings.bUseMikkTSpace = true;

	FMeshDescription* CurrentMeshDescription = StaticMesh->GetMeshDescription(0);
	if (!CurrentMeshDescription)
	{
		CurrentMeshDescription = StaticMesh->CreateMeshDescription(0);
	}
	*CurrentMeshDescription = MeshDescription;
	StaticMesh->CommitMeshDescription(0);

	TArray<FStaticMaterial> StaticMaterials;
	for (int32 i = 0; i < (int32)Model.Meshes.Num(); ++i)
	{
		FStaticMaterial&& Material = FStaticMaterial(UMaterial::GetDefaultMaterial(MD_Surface));

		uint32 CastMatIdx = SceneRoot.MaterialMap[Model.Meshes[i].MaterialHash];
		const FCastMaterialInfo& CastMaterial = SceneRoot.Materials[CastMatIdx];

		if (CastMaterial.Textures.Num() > 0)
		{
			UMaterialInterface* MaterialInstance =
				CreateMaterialInstance(CastMaterial, ParentPackage);
			Material = FStaticMaterial(MaterialInstance);
		}
		Material.UVChannelData.bInitialized = true;
		Material.MaterialSlotName = FName(CastMaterial.Name);
		Material.ImportedMaterialSlotName = FName(CastMaterial.Name);
		StaticMaterials.Add(Material);
		StaticMesh->GetSectionInfoMap().Set(0, i, FMeshSectionInfo(i));
	}
	StaticMesh->SetStaticMaterials(StaticMaterials);
	StaticMesh->GetOriginalSectionInfoMap().CopyFrom(StaticMesh->GetSectionInfoMap());
	StaticMesh->ImportVersion = LastVersion;
	StaticMesh->Build(false);
	StaticMesh->EnforceLightmapRestrictions();
	StaticMesh->PostEditChange();
	StaticMesh->GetPackage()->FullyLoad();
	StaticMesh->MarkPackageDirty();
}

UObject* FCastImporter::CreateAssetOfClass(UClass* AssetClass, FString ParentPackageName, FString ObjectName,
                                           bool bAllowReplace)
{
	FString ParentPath = FString::Printf(TEXT("%s/%s"), *FPackageName::GetLongPackagePath(*ParentPackageName),
	                                     *ObjectName);
	UObject* Parent = CreatePackage(*ParentPath);
	UObject* Object = LoadObject<UObject>(Parent, *ObjectName, NULL, LOAD_NoWarn | LOAD_Quiet, NULL);

	if (Object == NULL)
	{
		Object = NewObject<UObject>(Parent, AssetClass, *ObjectName, RF_Public | RF_Standalone);
		Object->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(Object);
	}

	return Object;
}

bool FCastImporter::ImportAnimation(USkeleton* Skeleton, UAnimSequence* DestSeq, FCastAnimationInfo& Animation)
{
	IAnimationDataController& Controller = DestSeq->GetController();
	InitializeAnimationController(Controller, Animation);

	TMap<FString, BoneCurve> BoneMap = ExtractCurves(Skeleton, Animation);

	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();

	if (!ImportOptions->bConvertRefPosition)
	{
		AddBoneCurves(Controller, RefSkeleton);
	}

	uint32 NumberOfFrames = CalculateNumberOfFrames(Animation);

	PopulateBoneTracks(Controller, BoneMap, RefSkeleton, NumberOfFrames);

	if (!Animation.NotificationTracks.IsEmpty() && ImportOptions->bImportAnimationNotify)
	{
		AddAnimationNotifies(DestSeq, Animation, Animation.Framerate);
	}

	FinalizeController(Controller, DestSeq);

	return true;
}

void FCastImporter::InitializeAnimationController(IAnimationDataController& Controller,
                                                  const FCastAnimationInfo& Animation)
{
	Controller.InitializeModel();
	Controller.OpenBracket(LOCTEXT("ImportAnimation_Bracket", "Importing Animation"), false);
	Controller.RemoveAllBoneTracks(false);
	float AnimationRate = Animation.Framerate ? Animation.Framerate : 30;
	Controller.SetFrameRate(FFrameRate(AnimationRate, 1), false);
	uint32 NumberOfFrames = CalculateNumberOfFrames(Animation);
	Controller.SetNumberOfFrames(FFrameNumber((int32)NumberOfFrames), false);
}

uint32 FCastImporter::CalculateNumberOfFrames(const FCastAnimationInfo& Animation)
{
	uint32 NumberOfFrames = 0;
	for (const FCastCurveInfo& Curve : Animation.Curves)
	{
		NumberOfFrames = FMath::Max(NumberOfFrames, Curve.KeyFrameBuffer.Last() + 1);
	}
	return NumberOfFrames;
}

TMap<FString, FCastImporter::BoneCurve> FCastImporter::ExtractCurves(USkeleton* Skeleton,
                                                                     const FCastAnimationInfo& Animation)
{
	TMap<FString, BoneCurve> BoneMap;

	FName RootName = Skeleton->GetReferenceSkeleton().GetBoneName(0);
	for (const FCastCurveInfo& Curve : Animation.Curves)
	{
		if (ImportOptions->bDeleteRootNodeAnim && Curve.NodeName == RootName)
		{
			continue;
		}
		BoneCurve& BoneCurveInfo = BoneMap.FindOrAdd(Curve.NodeName);
		if (Curve.Mode != "absolute")
		{
			BoneCurveInfo.AnimMode = ECastAnimImportType::CastAIT_Relative;
		}
		AssignCurveValues(BoneCurveInfo, Curve);
	}
	return BoneMap;
}

void FCastImporter::AssignCurveValues(BoneCurve& BoneCurveInfo, const FCastCurveInfo& Curve)
{
	if (Curve.KeyPropertyName == "tx")
	{
		InterpolateAnimationKeyframes(BoneCurveInfo.PositionX, Curve.KeyFrameBuffer, Curve.KeyValueBuffer);
	}
	else if (Curve.KeyPropertyName == "ty")
	{
		InterpolateAnimationKeyframes(BoneCurveInfo.PositionY, Curve.KeyFrameBuffer, Curve.KeyValueBuffer);
	}
	else if (Curve.KeyPropertyName == "tz")
	{
		InterpolateAnimationKeyframes(BoneCurveInfo.PositionZ, Curve.KeyFrameBuffer, Curve.KeyValueBuffer);
	}
	else if (Curve.KeyPropertyName == "rq")
	{
		InterpolateAnimationKeyframes(BoneCurveInfo.Rotation, Curve.KeyFrameBuffer, Curve.KeyValueBuffer);
	}
	else if (Curve.KeyPropertyName == "sx")
	{
		InterpolateAnimationKeyframes(BoneCurveInfo.ScaleX, Curve.KeyFrameBuffer, Curve.KeyValueBuffer);
	}
	else if (Curve.KeyPropertyName == "sy")
	{
		InterpolateAnimationKeyframes(BoneCurveInfo.ScaleY, Curve.KeyFrameBuffer, Curve.KeyValueBuffer);
	}
	else if (Curve.KeyPropertyName == "sz")
	{
		InterpolateAnimationKeyframes(BoneCurveInfo.ScaleZ, Curve.KeyFrameBuffer, Curve.KeyValueBuffer);
	}
}

void FCastImporter::AddBoneCurves(IAnimationDataController& Controller, const FReferenceSkeleton& RefSkeleton)
{
	const TArray<FName>& BoneNames = RefSkeleton.GetRawRefBoneNames();
	for (FName BoneName : BoneNames)
	{
		Controller.AddBoneCurve(BoneName, false);
	}
}

void FCastImporter::PopulateBoneTracks(IAnimationDataController& Controller, const TMap<FString, BoneCurve>& BoneMap,
                                       const FReferenceSkeleton& RefSkeleton, uint32 NumberOfFrames)
{
	for (const auto& BoneTransformKeys : BoneMap)
	{
		FName NewCurveName(BoneTransformKeys.Key);
		TArray<FVector3f> PositionalKeys;
		TArray<FQuat4f> RotationalKeys;
		TArray<FVector3f> ScalingKeys;

		int32 BoneID = RefSkeleton.FindBoneIndex(NewCurveName);
		FVector BoneLocation = {0, 0, 0};
		FQuat BoneRotation = {0, 0, 0, 1};
		FVector BoneScale = {1, 1, 1};

		if (RefSkeleton.GetRefBonePose().IsValidIndex(BoneID))
		{
			BoneLocation = RefSkeleton.GetRefBonePose()[BoneID].GetLocation();
			BoneRotation = RefSkeleton.GetRefBonePose()[BoneID].GetRotation();
			BoneScale = RefSkeleton.GetRefBonePose()[BoneID].GetScale3D();
		}

		SetPositionalKeys(PositionalKeys, BoneTransformKeys.Value, BoneLocation, NumberOfFrames);
		SetRotationalKeys(RotationalKeys, BoneTransformKeys.Value, BoneRotation, RefSkeleton, BoneID, NumberOfFrames);
		SetScalingKeys(ScalingKeys, BoneTransformKeys.Value, BoneScale, NumberOfFrames);

		if (ImportOptions->bConvertRefPosition)
		{
			Controller.AddBoneCurve(NewCurveName, false);
		}
		Controller.SetBoneTrackKeys(NewCurveName, PositionalKeys, RotationalKeys, ScalingKeys);
	}
}

void FCastImporter::SetPositionalKeys(TArray<FVector3f>& PositionalKeys, const BoneCurve& BoneCurveInfo,
                                      const FVector& BoneLocation, uint32 NumberOfFrames)
{
	for (int32 i = 0; i < BoneCurveInfo.PositionX.Num(); ++i)
	{
		if (ImportOptions->bConvertRefPosition)
		{
			PositionalKeys.Add({
				BoneCurveInfo.PositionX[i] + (float)BoneLocation.X,
				-(BoneCurveInfo.PositionY[i] + (float)BoneLocation.Y),
				BoneCurveInfo.PositionZ[i] + (float)BoneLocation.Z
			});
		}
		else
		{
			PositionalKeys.Add({
				BoneCurveInfo.PositionX[i],
				-BoneCurveInfo.PositionY[i],
				BoneCurveInfo.PositionZ[i]
			});
		}
	}

	if (PositionalKeys.IsEmpty())
	{
		if (ImportOptions->bConvertRefPosition)
		{
			PositionalKeys.Add({(float)BoneLocation.X, (float)BoneLocation.Y, (float)BoneLocation.Z});
		}
		else
		{
			PositionalKeys.AddZeroed();
		}
	}

	ExtendToFrameCount(PositionalKeys, NumberOfFrames);
}

void FCastImporter::SetRotationalKeys(TArray<FQuat4f>& RotationalKeys, const BoneCurve& BoneCurveInfo,
                                      const FQuat& BoneRotation, const FReferenceSkeleton& RefSkeleton, int32 BoneID,
                                      uint32 NumberOfFrames)
{
	for (int32 i = 0; i < BoneCurveInfo.Rotation.Num(); ++i)
	{
		FQuat4f CurrentBoneRotation{
			(float)BoneCurveInfo.Rotation[i].X,
			-(float)BoneCurveInfo.Rotation[i].Y,
			(float)BoneCurveInfo.Rotation[i].Z,
			-(float)BoneCurveInfo.Rotation[i].W
		};

		if (ImportOptions->bConvertRefAnim && BoneID != INDEX_NONE)
		{
			FQuat4f RefRotation = FQuat4f(RefSkeleton.GetRefBonePose()[BoneID].GetRotation());
			CurrentBoneRotation = RefRotation * CurrentBoneRotation;
		}

		RotationalKeys.Add(CurrentBoneRotation);
		RotationalKeys.Last().Normalize();
	}

	if (RotationalKeys.IsEmpty())
	{
		RotationalKeys.Add({
			(float)BoneRotation.X, (float)BoneRotation.Y, (float)BoneRotation.Z, (float)BoneRotation.W
		});
	}

	ExtendToFrameCount(RotationalKeys, NumberOfFrames);
}

void FCastImporter::SetScalingKeys(TArray<FVector3f>& ScalingKeys, const BoneCurve& BoneCurveInfo,
                                   const FVector& BoneScale, uint32 NumberOfFrames)
{
	for (int32 i = 0; i < BoneCurveInfo.ScaleX.Num(); ++i)
	{
		ScalingKeys.Add({
			BoneCurveInfo.ScaleX[i],
			BoneCurveInfo.ScaleY[i],
			BoneCurveInfo.ScaleZ[i],
		});
	}

	if (ScalingKeys.IsEmpty())
	{
		ScalingKeys.Add({(float)BoneScale.X, (float)BoneScale.Y, (float)BoneScale.Z});
	}

	ExtendToFrameCount(ScalingKeys, NumberOfFrames);
}

void FCastImporter::AddAnimationNotifies(UAnimSequence* DestSeq, const FCastAnimationInfo& Animation,
                                         float AnimationRate)
{
	for (const FCastNotificationTrackInfo& NotificationTrack : Animation.NotificationTracks)
	{
		for (uint32 KeyFrame : NotificationTrack.KeyFrameBuffer)
		{
			FAnimNotifyEvent NotifyEvent;
			NotifyEvent.NotifyName = *NotificationTrack.Name;
			NotifyEvent.SetTime(KeyFrame / AnimationRate);
			DestSeq->Notifies.Add(NotifyEvent);
		}
	}
}

void FCastImporter::FinalizeController(IAnimationDataController& Controller, UAnimSequence* DestSeq)
{
	Controller.NotifyPopulated();
	Controller.CloseBracket(false);

	DestSeq->Modify(true);
	DestSeq->PostEditChange();

	FAssetRegistryModule::AssetCreated(DestSeq);
	DestSeq->MarkPackageDirty();
}

FString FCastImporter::NoIllegalSigns(const FString& InString)
{
	return InString.Replace(TEXT("~"), TEXT("_")).Replace(TEXT("$"), TEXT("_")).Replace(TEXT("&"), TEXT("_")).Replace(
		TEXT("#"), TEXT("_"));
}

#undef LOCTEXT_NAMESPACE
