#include "Factories/CastAssetFactory.h"

#include "Misc/ScopedSlowTask.h"
#include "EditorReimportHandler.h"
#include "FileHelpers.h"
#include "PackageTools.h"
#include "Widgets/CastImportUI.h"
#include "SeLogChannels.h"
#include "CastManager/CastReader.h"
#include "CastManager/CastToUnrealConverter.h"
#include "CastManager/DefaultCastAnimationImporter.h"
#include "CastManager/DefaultCastMaterialImporter.h"
#include "CastManager/DefaultCastMaterialParser.h"
#include "CastManager/DefaultCastMeshImporter.h"
#include "CastManager/DefaultCastTextureImporter.h"
#include "Utils/CastImporter.h"
#include "Widgets/SlateCastImportUIHandler.h"

#define LOCTEXT_NAMESPACE "CastFactory"

UCastAssetFactory::UCastAssetFactory(const FObjectInitializer& ObjectInitializer): Super(ObjectInitializer)
{
	Formats.Add(TEXT("cast; models, animations, and more"));

	bCreateNew = false;
	bEditorImport = true;
	SupportedClass = nullptr;

	InitializeDependencies();

	ImportUI = NewObject<UCastImportUI>();

	// bText = false;
	// bShowOption = true;
	// bDetectImportTypeOnImport = true;
}

void UCastAssetFactory::PostInitProperties()
{
	Super::PostInitProperties();
	// TODO: Load last dialog state 加载上次对话框设置
}

bool UCastAssetFactory::ConfigureProperties()
{
	InitializeDependencies();

	bImportAll = false;

	bool bProceed = UIHandler->GetImportOptions(ImportOptions, true, IsAutomatedImport(), GetCurrentFilename(),
	                                            bImportAll);

	return bProceed;
}

bool UCastAssetFactory::DoesSupportClass(UClass* Class)
{
	// return Class == UStaticMesh::StaticClass() ||
	// Class == USkeletalMesh::StaticClass() ||
	// Class == UAnimSequence::StaticClass();
	return false;
}

FText UCastAssetFactory::GetDisplayName() const
{
	return NSLOCTEXT("CastImporter", "FactoryDisplayName", "CAST Asset");
}

UClass* UCastAssetFactory::ResolveSupportedClass()
{
	return UStaticMesh::StaticClass();
}

UObject* UCastAssetFactory::HandleExistingAsset(UObject* InParent, FName InName, const FString& InFilename)
{
	UObject* ExistingObject = nullptr;
	if (InParent)
	{
		ExistingObject = StaticFindObject(UObject::StaticClass(), InParent, *InName.ToString());
		if (ExistingObject)
		{
			// TODO: 重新导入
			FReimportHandler* ReimportHandler = nullptr;
			UFactory* ReimportHandlerFactory = nullptr;
			UAssetImportTask* HandlerOriginalImportTask = nullptr;
			bool bIsObjectSupported = false;
			UE_LOG(LogCast, Warning, TEXT("'%s' already exists and cannot be imported"), *InFilename)
		}
	}
	return ExistingObject;
}

void UCastAssetFactory::HandleMaterialImport(FString&& ParentPath, const FString& InFilename,
                                             FCastImporter* CastImporter,
                                             FCastImportOptions* ImportOptions)
{
	FString FilePath = FPaths::GetPath(InFilename);
	FString TextureBasePath;
	FString TextureFormat = ImportOptions->TextureFormat;

	if (ImportOptions->TexturePathType == ECastTextureImportType::CastTIT_Default)
	{
		TextureBasePath = FPaths::Combine(FilePath, TEXT("_images"));
		CastImporter->AnalysisMaterial(ParentPath, FilePath, TextureBasePath, TextureFormat);
	}
	else if (ImportOptions->TexturePathType == ECastTextureImportType::CastTIT_GlobalMaterials)
	{
		TextureBasePath = ImportOptions->GlobalMaterialPath;
		CastImporter->AnalysisMaterial(ParentPath, FilePath, TextureBasePath, TextureFormat);
	}
	else if (ImportOptions->TexturePathType == ECastTextureImportType::CastTIT_GlobalImages)
	{
		TextureBasePath = ImportOptions->GlobalMaterialPath;
		CastImporter->AnalysisMaterial(ParentPath, FilePath, TextureBasePath, TextureFormat, true);
	}
}

UObject* UCastAssetFactory::ExecuteImportProcess(UObject* InParent, FName InName, EObjectFlags Flags,
                                                 const FString& InFilename, FCastImporter* CastImporter,
                                                 FCastImportOptions* ImportOptions, FString InCurrentFilename)
{
	UObject* CreatedObject = nullptr;
	if (!CastImporter->ImportFromFile(InCurrentFilename))
	{
		UE_LOG(LogCast, Error, TEXT("Fail to Import Cast File"));
	}
	else
	{
		if (ImportOptions->bImportMaterial && CastImporter->SceneInfo.TotalMaterialNum > 0)
		{
			FString ParentPath = FPaths::GetPath(InParent->GetPathName());
			HandleMaterialImport(MoveTemp(ParentPath), InFilename, CastImporter, ImportOptions);
		}
		if (!ImportOptions->bImportAsSkeletal && ImportOptions->bImportMesh)
		{
			UStaticMesh* NewStaticMesh = CastImporter->ImportStaticMesh(InParent, InName, Flags);
			CreatedObject = NewStaticMesh;
		}
		else if (ImportOptions->bImportMesh && CastImporter->SceneInfo.TotalGeometryNum > 0)
		{
			UPackage* Package = Cast<UPackage>(InParent);

			FName OutputName = *FPaths::GetBaseFilename(InFilename);

			CastScene::FImportSkeletalMeshArgs ImportSkeletalMeshArgs;
			ImportSkeletalMeshArgs.InParent = Package;
			ImportSkeletalMeshArgs.Name = OutputName;
			ImportSkeletalMeshArgs.Flags = Flags;

			USkeletalMesh* BaseSkeletalMesh = CastImporter->ImportSkeletalMesh(ImportSkeletalMeshArgs);
			CreatedObject = BaseSkeletalMesh;
		}
		else if (ImportOptions->bImportAnimations && CastImporter->SceneInfo.bHasAnimation)
		{
			CreatedObject = CastImporter->ImportAnim(InParent, ImportOptions->Skeleton);
		}
	}
	return CreatedObject;
}

UObject* UCastAssetFactory::FactoryCreateFile(
	UClass* InClass,
	UObject* InParent,
	FName InName,
	EObjectFlags Flags,
	const FString& InFilename,
	const TCHAR* Parms,
	FFeedbackContext* Warn,
	bool& bOutOperationCanceled)
{
	InitializeDependencies();

	const FCastImportOptions& CurrentOptions = ImportOptions;

	// 检查包名是否有效
	FText PackageNameError;
	if (!FPackageName::IsValidLongPackageName(InParent->GetPathName(), false, &PackageNameError))
	{
		Warn->Logf(ELogVerbosity::Error, TEXT("Invalid package name: %s. %s"), *InParent->GetPathName(),
		           *PackageNameError.ToString());
		bOutOperationCanceled = true;
		return nullptr;
	}

	const int32 TotalSteps = 100;
	FScopedSlowTask SlowTask(
		TotalSteps, GetImportTaskText(NSLOCTEXT("CastFactory", "BeginImportCastFile", "Opening Cast file.")), true);

	if (Warn->GetScopeStack().Num() == 0)
	{
		SlowTask.MakeDialog(true);
	}
	// 验证
	SlowTask.EnterProgressFrame(5, GetImportTaskText(NSLOCTEXT("CastFactory", "ValidatingFile", "Validating File...")));

	// --- 文件验证 ---
	if (!IFileManager::Get().FileExists(*InFilename))
	{
		return HandleImportFailure(FString::Printf(TEXT("File '%s' not exists"), *InFilename), this, Warn,
		                           bOutOperationCanceled);
	}

	if (bOperationCanceled)
	{
		return HandleImportFailure(
			TEXT("The operation was canceled by the user before import"), this, Warn, bOutOperationCanceled);
	}

	// --- 广播 PreImport 事件 ---
	FString FileExtension = FPaths::GetExtension(InFilename);
	const TCHAR* Type = *FileExtension;
	if (GEditor)
	{
		GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPreImport(this, InClass, InParent, InName, Type);
	}

	// --- 核心导入流程 ---
	FOnCastImportProgress ProgressDelegate;
	ProgressDelegate.BindLambda([&SlowTask, Warn, this](float Percent, const FText& Status)
	{
		float OverallProgressFraction = 0.1f + Percent * 0.8f;
		SlowTask.EnterProgressFrame(0, GetImportTaskText(Status));
	});
	Converter->SetProgressDelegate(ProgressDelegate);

	ON_SCOPE_EXIT
	{
		if (Reader) Reader->Cleanup();
	};

	// 加载和导入 Cast 文件数据
	SlowTask.EnterProgressFrame(
		5, GetImportTaskText(NSLOCTEXT("CastFactory", "LoadingFile", "Load cast file data...")));
	if (!Reader->LoadFile(InFilename))
	{
		return HandleImportFailure(
			FString::Printf(TEXT("Failed to load file: %s"), *InFilename), this, Warn, bOutOperationCanceled);
	}

	SlowTask.EnterProgressFrame(5, GetImportTaskText(NSLOCTEXT("CastFactory", "ImportingScene", "导入场景数据...")));
	if (!Reader->ImportSceneData())
	{
		return HandleImportFailure(FString::Printf(TEXT("Failed to import scene data: %s"), *InFilename),
		                           this, Warn, bOutOperationCanceled);
	}

	// 转换 Cast 数据到 Unreal 资源
	SlowTask.EnterProgressFrame(0, GetImportTaskText(NSLOCTEXT("CastFactory", "ConvertingData", "转换资源...")));
	FCastScene* SceneData = Reader->GetCastScene();
	if (!SceneData)
	{
		return HandleImportFailure(TEXT("无法从 Reader 获取场景数据"), this, Warn, bOutOperationCanceled);
	}

	TArray<UObject*> OtherCreated;
	UObject* ResultObject = Converter->Convert(*SceneData, CurrentOptions, InParent, InName, Flags | RF_Transactional,
	                                           InFilename, OtherCreated);

	if (bOperationCanceled || ResultObject == nullptr && SceneData->GetMeshNum() > 0)
	{
		if (!ResultObject && OtherCreated.IsEmpty())
		{
			return HandleImportFailure(bOperationCanceled ? TEXT("操作在转换过程中被用户取消") : TEXT("转换过程失败，未创建任何资源"),
			                           this, Warn, bOutOperationCanceled);
		}
	}

	TArray<UObject*> AllCreatedObjects;
	if (ResultObject)
	{
		AllCreatedObjects.Add(ResultObject);
	}
	AllCreatedObjects.Append(OtherCreated);

	SlowTask.EnterProgressFrame(5, GetImportTaskText(NSLOCTEXT("CastFactory", "FinalizingImport", "完成导入...")));

	UObject* BroadcastObject = ResultObject
		                           ? ResultObject
		                           : (AllCreatedObjects.Num() > 0 ? AllCreatedObjects[0] : nullptr);
	if (GEditor)
	{
		GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, BroadcastObject);
	}


	// 保存创建的包
	if (!AllCreatedObjects.IsEmpty())
	{
		SaveCreatedPackages(AllCreatedObjects);
	}
	else if (!bOperationCanceled)
	{
		Warn->Logf(ELogVerbosity::Warning, TEXT("导入过程完成，但未创建任何资源对象。"));
	}

	// 导入后打开编辑器
	if (ResultObject)
	{
		UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		AssetEditorSubsystem->OpenEditorForAsset(ResultObject);
	}

	return ResultObject;
}

bool UCastAssetFactory::DetectImportType(const FString& InFilename)
{
	FCastImporter* CastImporter = FCastImporter::GetInstance();
	int32 ImportType = CastImporter->GetImportType(InFilename);
	if (ImportType == -1)
	{
		CastImporter->ReleaseScene();
		return false;
	}

	return true;
}

FText UCastAssetFactory::GetImportTaskText(const FText& TaskText) const
{
	return bOperationCanceled ? LOCTEXT("CancellingImportingCastTask", "Cancelling Cast import") : TaskText;
}

void UCastAssetFactory::InitializeDependencies()
{
	if (!Reader) Reader = MakeShared<FCastReader>();

	if (!Converter)
	{
		TSharedPtr<FDefaultCastMaterialParser> DefaultMaterialParser = MakeShared<FDefaultCastMaterialParser>();
		TSharedPtr<FDefaultCastTextureImporter> DefaultTextureImporter = MakeShared<FDefaultCastTextureImporter>();

		TSharedPtr<FDefaultCastMaterialImporter> DefaultMaterialImporter = MakeShared<FDefaultCastMaterialImporter>();

		TSharedPtr<FDefaultCastMeshImporter> DefaultMeshImporter = MakeShared<FDefaultCastMeshImporter>();
		TSharedPtr<FDefaultCastAnimationImporter> DefaultAnimationImporter = MakeShared<
			FDefaultCastAnimationImporter>();
		Converter = MakeShared<FCastToUnrealConverter>(DefaultMaterialParser, DefaultTextureImporter,
		                                               DefaultMaterialImporter, DefaultMeshImporter,
		                                               DefaultAnimationImporter);
	}

	if (!UIHandler) UIHandler = MakeShared<FSlateCastImportUIHandler>();

	UE_LOG(LogCast, Log, TEXT("CastAssetFactory dependencies initialized."));
}

void UCastAssetFactory::SaveCreatedPackages(const TArray<UObject*>& CreatedObjects)
{
	TArray<UPackage*> PackagesToSave;
	for (UObject* Obj : CreatedObjects)
	{
		if (Obj)
		{
			UPackage* Package = Obj->GetOutermost();
			if (Package && Package != GetTransientPackage()) // Don't save transient package
			{
				PackagesToSave.AddUnique(Package);
			}
		}
	}

	if (!PackagesToSave.IsEmpty())
	{
		UE_LOG(LogCast, Log, TEXT("Saving %d packages..."), PackagesToSave.Num());
		const bool bCheckDirty = false;
		const bool bPromptToSave = false;
		FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, bCheckDirty, bPromptToSave);
	}
}

UObject* UCastAssetFactory::HandleImportFailure(const FString& ErrorMessage, UFactory* Factory, FFeedbackContext* Warn,
                                                bool& bOutOperationCanceled)
{
	Warn->Logf(ELogVerbosity::Error, TEXT("%s"), *ErrorMessage);
	UE_LOG(LogCast, Error, TEXT("%s"), *ErrorMessage);
	bOutOperationCanceled = true;
	if (GEditor)
	{
		GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(Factory, nullptr);
	}
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
