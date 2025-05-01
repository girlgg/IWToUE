#include "CastManager/DefaultCastMaterialImporter.h"

#include "CastManager/CastImportOptions.h"
#include "CastManager/CastModel.h"
#include "CastManager/DefaultMaterialCreationStrategy_IW8.h"
#include "CastManager/DefaultMaterialCreationStrategy_IW9.h"
#include "CastManager/DefaultMaterialCreationStrategy_T10.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Materials/MaterialInstanceConstant.h"

FDefaultCastMaterialImporter::FDefaultCastMaterialImporter()
{
	MaterialStrategies.Add(ECastMaterialType::CastMT_IW9, MakeShared<FDefaultMaterialCreationStrategy_IW9>());
	MaterialStrategies.Add(ECastMaterialType::CastMT_IW8, MakeShared<FDefaultMaterialCreationStrategy_IW8>());
	// MaterialStrategies.Add(ECastMaterialType::CastMT_T7, MakeShared<FDefaultMaterialCreationStrategy_T7>());
	MaterialStrategies.Add(ECastMaterialType::CastMT_T10, MakeShared<FDefaultMaterialCreationStrategy_T10>());
	UE_LOG(LogCast, Log, TEXT("Material Importer Initialized with %d strategies."), MaterialStrategies.Num());
}

UMaterialInterface* FDefaultCastMaterialImporter::CreateMaterialInstance(const FCastMaterialInfo& MaterialInfo,
                                                                         const FCastImportOptions& Options,
                                                                         UObject* ParentPackage)
{
	ReportProgress(0.0f, FText::Format(
		               NSLOCTEXT("CastImporter", "MaterialCreating", "Creating Material {0}"),
		               FText::FromString(MaterialInfo.Name)));

	FPreparedMaterialInstanceData PreparedData = PrepareMaterialInstanceData_OffThread(
		MaterialInfo, Options, ParentPackage);
	if (!PreparedData.bIsValid)
	{
		UE_LOG(LogCast, Error, TEXT("Failed to prepare material instance data for %s."), *MaterialInfo.Name);
		ReportProgress(1.0f, FText::Format(
			               NSLOCTEXT("CastImporter", "MaterialFailedNoStrategy", "Material Failed (No Strategy) {0}"),
			               FText::FromString(MaterialInfo.Name)));
		return nullptr;
	}

	UMaterialInstanceConstant* ResultMaterialInstance = nullptr;

	if (IsInGameThread())
	{
		ResultMaterialInstance = ApplyMaterialInstanceData_GameThread(PreparedData);
	}
	else
	{
		TPromise<UMaterialInstanceConstant*> Promise;
		TFuture<UMaterialInstanceConstant*> Future = Promise.GetFuture();

		AsyncTask(ENamedThreads::GameThread,
		          [this, Promise = MoveTemp(Promise), PreparedData = MoveTemp(PreparedData)]() mutable
		          {
			          UMaterialInstanceConstant* Result = ApplyMaterialInstanceData_GameThread(PreparedData);
			          Promise.SetValue(Result);
		          });

		ResultMaterialInstance = Future.Get();
	}

	return ResultMaterialInstance;
}

void FDefaultCastMaterialImporter::SetTextureParameter(UMaterialInstanceConstant* Instance,
                                                       const FCastTextureInfo& TextureInfo)
{
	if (!Instance || !TextureInfo.TextureObject || TextureInfo.TextureType.IsEmpty())
	{
		if (!TextureInfo.TextureObject && !TextureInfo.TextureType.IsEmpty())
		{
			UE_LOG(LogCast, Warning,
			       TEXT(
				       "Cannot set texture parameter '%s' for material '%s': Texture Object is null (Import likely failed)."
			       ), *TextureInfo.TextureType, *Instance->GetName());
		}
		return;
	}

	if (UTexture* TextureAsset = Cast<UTexture>(TextureInfo.TextureObject))
	{
		FMaterialParameterInfo TextureParameterInfo(FName(*TextureInfo.TextureType), GlobalParameter, -1);
		Instance->SetTextureParameterValueEditorOnly(TextureParameterInfo, TextureAsset);
	}
	else
	{
		UE_LOG(LogCast, Warning, TEXT("TextureObject for parameter '%s' in material '%s' is not a UTexture."),
		       *TextureInfo.TextureType, *Instance->GetName());
	}
}

FPreparedMaterialInstanceData FDefaultCastMaterialImporter::PrepareMaterialInstanceData_OffThread(
	const FCastMaterialInfo& MaterialInfo, const FCastImportOptions& Options,
	UObject* ParentPackage)
{
	FPreparedMaterialInstanceData PreparedData;
	PreparedData.OriginalMaterialInfo = MaterialInfo;

	PreparedData.SanitizedMaterialName = NoIllegalSigns(MaterialInfo.Name);
	FString ParentPackagePath = FPaths::GetPath(ParentPackage->GetPathName());
	FString MaterialPackagePath = FPaths::Combine(ParentPackagePath, TEXT("Materials"));
	FString MaterialAssetPath = FPaths::Combine(MaterialPackagePath, PreparedData.SanitizedMaterialName);
	PreparedData.FinalPackageName = FPackageName::ObjectPathToPackageName(MaterialAssetPath);

	PreparedData.Strategy = GetStrategy(Options.MaterialType);
	if (!PreparedData.Strategy)
	{
		UE_LOG(LogCast, Error,
		       TEXT("Prepare: No material creation strategy found for type %d. Cannot create material %s."),
		       (int)Options.MaterialType, *PreparedData.SanitizedMaterialName);
		return PreparedData;
	}

	PreparedData.ParentMaterialPath = PreparedData.Strategy->GetParentMaterialPath(
		MaterialInfo, PreparedData.bParentMaterialIsMetallic);
	if (PreparedData.ParentMaterialPath.IsEmpty())
	{
		UE_LOG(LogCast, Error, TEXT("Prepare: Strategy failed to provide parent material path for %s."),
		       *PreparedData.SanitizedMaterialName);
		return PreparedData;
	}

	for (const FCastTextureInfo& TexInfo : MaterialInfo.Textures)
	{
		if (!TexInfo.TextureType.IsEmpty() && TexInfo.TextureObject != nullptr)
		{
			PreparedData.TextureParametersToSet.Emplace(FName(*TexInfo.TextureType), TexInfo.TextureObject);
		}
		else
		{
			UE_LOG(LogCast, Warning,
			       TEXT("Prepare_V2: Skipping texture parameter '%s' with null TextureObject for material %s."),
			       *TexInfo.TextureType, *PreparedData.SanitizedMaterialName);
		}
	}

	for (const FCastSettingInfo& Setting : MaterialInfo.Settings)
	{
		if (Setting.Name.IsEmpty()) continue;
		FName ParameterName(*Setting.Name);

		switch (Setting.Type)
		{
		case Float1:
			PreparedData.ScalarParametersToSet.Emplace(ParameterName, Setting.Value.X);
			break;
		case Float2:
			PreparedData.VectorParametersToSet.Emplace(ParameterName,
			                                           FLinearColor(Setting.Value.X, Setting.Value.Y, 0.0f, 0.0f));
			break;
		case Float3:
			PreparedData.VectorParametersToSet.Emplace(ParameterName,
			                                           FLinearColor(Setting.Value.X, Setting.Value.Y, Setting.Value.Z,
			                                                        0.0f));
			break;
		case Color:
		case Float4:
			PreparedData.VectorParametersToSet.Emplace(ParameterName,
			                                           FLinearColor(Setting.Value.X, Setting.Value.Y, Setting.Value.Z,
			                                                        Setting.Value.W));
			break;
		default:
			UE_LOG(LogCast, Warning, TEXT("Prepare: Unhandled setting type %d for parameter %s in material %s"),
			       (int)Setting.Type, *Setting.Name, *PreparedData.SanitizedMaterialName);
			break;
		}
	}

	PreparedData.bIsValid = true;
	return PreparedData;
}

UMaterialInstanceConstant* FDefaultCastMaterialImporter::ApplyMaterialInstanceData_GameThread(
	FPreparedMaterialInstanceData& PreparedData)
{
	if (!PreparedData.bIsValid || !PreparedData.Strategy)
	{
		UE_LOG(LogCast, Error, TEXT("GT_Apply: Prepared data is invalid or strategy is missing for %s."),
		       *PreparedData.SanitizedMaterialName);
		return nullptr;
	}
	ReportProgress(0.0f, FText::Format(
		               NSLOCTEXT("CastImporter", "MaterialCreatingGT", "Creating Material {0} (GT)"),
		               FText::FromString(PreparedData.SanitizedMaterialName)));

	// --- Load Parent Material (Game Thread) ---
	UMaterial* ParentMaterial = LoadObject<UMaterial>(nullptr, *PreparedData.ParentMaterialPath);
	if (!ParentMaterial)
	{
		UE_LOG(LogCast, Error, TEXT("GT_Apply: Failed to load parent material '%s' for material %s."),
		       *PreparedData.ParentMaterialPath, *PreparedData.SanitizedMaterialName);
		ReportProgress(1.0f, FText::Format(
			               NSLOCTEXT("CastImporter", "MaterialFailedNoParentGT", "Material Failed (No Parent) {0}"),
			               FText::FromString(PreparedData.SanitizedMaterialName)));
		return nullptr;
	}

	// --- Create Package (Game Thread) ---
	UPackage* InstancePackage = CreatePackage(*PreparedData.FinalPackageName);
	if (!InstancePackage)
	{
		UE_LOG(LogCast, Error, TEXT("GT_Apply: Failed to create package for material instance: %s"),
		       *PreparedData.FinalPackageName);
		ReportProgress(1.0f, FText::Format(
			               NSLOCTEXT("CastImporter", "MaterialFailedPackageGT", "Material Failed (Package) {0}"),
			               FText::FromString(PreparedData.SanitizedMaterialName)));
		return nullptr;
	}
	InstancePackage->FullyLoad();
	InstancePackage->Modify();

	// --- Create Material Instance (Game Thread) ---
	UMaterialInstanceConstantFactoryNew* MaterialInstanceFactory = NewObject<UMaterialInstanceConstantFactoryNew>();
	MaterialInstanceFactory->InitialParent = ParentMaterial;

	ReportProgress(0.2f, FText::Format(
		               NSLOCTEXT("CastImporter", "MaterialFactoryCreateGT", "Factory Creating {0} (GT)"),
		               FText::FromString(PreparedData.SanitizedMaterialName)));
	UMaterialInstanceConstant* MaterialInstance = Cast<UMaterialInstanceConstant>(
		MaterialInstanceFactory->FactoryCreateNew(
			UMaterialInstanceConstant::StaticClass(),
			InstancePackage,
			FName(*PreparedData.SanitizedMaterialName),
			RF_Public | RF_Standalone | RF_Transactional,
			nullptr, GWarn
		)
	);

	if (!MaterialInstance)
	{
		UE_LOG(LogCast, Error, TEXT("GT_Apply: MaterialInstanceFactory failed to create instance for %s."),
		       *PreparedData.SanitizedMaterialName);
		ReportProgress(1.0f, FText::Format(
			               NSLOCTEXT("CastImporter", "MaterialFailedFactoryGT", "Material Failed (Factory) {0}"),
			               FText::FromString(PreparedData.SanitizedMaterialName)));
		return nullptr;
	}

	FAssetRegistryModule::AssetCreated(MaterialInstance);

	// --- Apply Parameters (Game Thread) ---
	MaterialInstance->Modify();
	MaterialInstance->PreEditChange(nullptr);

	ReportProgress(0.5f, FText::Format(
		               NSLOCTEXT("CastImporter", "MaterialSetParamsGT", "Setting Parameters {0} (GT)"),
		               FText::FromString(PreparedData.SanitizedMaterialName)));

	// Set Texture Parameters (Requires loading UTexture on GT)
	for (const auto& TexParamTuple : PreparedData.TextureParametersToSet)
	{
		FName ParameterName = TexParamTuple.Get<0>();
		UObject* TextureObjectPtr = TexParamTuple.Get<1>();

		if (TextureObjectPtr)
		{
			if (UTexture* TextureAsset = Cast<UTexture>(TextureObjectPtr))
			{
				if (IsValid(TextureAsset) && TextureAsset->GetResource())
				{
					FMaterialParameterInfo TextureParameterInfo(ParameterName);
					MaterialInstance->SetTextureParameterValueEditorOnly(TextureParameterInfo, TextureAsset);
				}
				else
				{
					UE_LOG(LogCast, Warning,
					       TEXT(
						       "GT_Apply: Texture object '%s' for parameter '%s' in material '%s' is invalid or has no resource."
					       ),
					       *TextureObjectPtr->GetName(), *ParameterName.ToString(),
					       *PreparedData.SanitizedMaterialName);
				}
			}
			else
			{
				UE_LOG(LogCast, Warning,
				       TEXT(
					       "GT_Apply: TextureObject passed for parameter '%s' in material '%s' could not be cast to UTexture."
				       ),
				       *ParameterName.ToString(), *PreparedData.SanitizedMaterialName);
			}
		}
	}

	for (const auto& ScalarParamTuple : PreparedData.ScalarParametersToSet)
	{
		FMaterialParameterInfo ParameterInfo(ScalarParamTuple.Get<0>());
		MaterialInstance->SetScalarParameterValueEditorOnly(ParameterInfo, ScalarParamTuple.Get<1>());
	}

	for (const auto& VectorParamTuple : PreparedData.VectorParametersToSet)
	{
		FMaterialParameterInfo ParameterInfo(VectorParamTuple.Get<0>());
		MaterialInstance->SetVectorParameterValueEditorOnly(ParameterInfo, VectorParamTuple.Get<1>());
	}

	PreparedData.Strategy->ApplyAdditionalParameters(MaterialInstance, PreparedData.OriginalMaterialInfo,
	                                                 PreparedData.bParentMaterialIsMetallic);

	MaterialInstance->PostEditChange();
	InstancePackage->MarkPackageDirty();

	ReportProgress(1.0f, FText::Format(
		               NSLOCTEXT("CastImporter", "MaterialCreatedGT", "Created Material {0} (GT)"),
		               FText::FromString(PreparedData.SanitizedMaterialName)));
	return MaterialInstance;
}

IMaterialCreationStrategy* FDefaultCastMaterialImporter::GetStrategy(ECastMaterialType Type) const
{
	const TSharedPtr<IMaterialCreationStrategy>* Found = MaterialStrategies.Find(Type);
	return Found ? Found->Get() : nullptr;
}
