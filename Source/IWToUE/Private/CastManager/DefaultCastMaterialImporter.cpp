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
	FString FullMaterialName = NoIllegalSigns(MaterialInfo.Name);
	FString ParentPackagePath = FPaths::GetPath(ParentPackage->GetPathName());
	FString MaterialPackagePath = FPaths::Combine(ParentPackagePath, TEXT("Materials"));
	FString MaterialAssetPath = FPaths::Combine(MaterialPackagePath, FullMaterialName);
	FString FinalPackageName = FPackageName::ObjectPathToPackageName(MaterialAssetPath);

	ReportProgress(0.0f, FText::Format(
		               NSLOCTEXT("CastImporter", "MaterialCreating", "Creating Material {0}"),
		               FText::FromString(FullMaterialName)));

	// Find Strategy
	IMaterialCreationStrategy* Strategy = GetStrategy(Options.MaterialType);
	if (!Strategy)
	{
		UE_LOG(LogCast, Error, TEXT("No material creation strategy found for type %d. Cannot create material %s."),
		       (int)Options.MaterialType, *FullMaterialName);
		ReportProgress(1.0f, FText::Format(
			               NSLOCTEXT("CastImporter", "MaterialFailedNoStrategy", "Material Failed (No Strategy) {0}"),
			               FText::FromString(FullMaterialName)));
		return nullptr;
	}

	// Determine Parent Material using Strategy
	bool bIsMetallic = false;
	FString ParentMaterialPath = Strategy->GetParentMaterialPath(MaterialInfo, bIsMetallic);
	UMaterial* ParentMaterial = LoadObject<UMaterial>(nullptr, *ParentMaterialPath);

	if (!ParentMaterial)
	{
		UE_LOG(LogCast, Error, TEXT("Failed to load parent material '%s' for material %s."), *ParentMaterialPath,
		       *FullMaterialName);
		ReportProgress(1.0f, FText::Format(
			               NSLOCTEXT("CastImporter", "MaterialFailedNoParent", "Material Failed (No Parent) {0}"),
			               FText::FromString(FullMaterialName)));
		return nullptr;
	}

	// --- Package and Asset Creation ---

	// Create Package for the instance
	UPackage* InstancePackage = CreatePackage(*FinalPackageName);
	if (!InstancePackage)
	{
		UE_LOG(LogCast, Error, TEXT("Failed to create package for material instance: %s"), *FinalPackageName);
		ReportProgress(1.0f, FText::Format(
			               NSLOCTEXT("CastImporter", "MaterialFailedPackage", "Material Failed (Package) {0}"),
			               FText::FromString(FullMaterialName)));
		return nullptr;
	}

	// Create Material Instance using Factory
	UMaterialInstanceConstantFactoryNew* MaterialInstanceFactory = NewObject<UMaterialInstanceConstantFactoryNew>();
	MaterialInstanceFactory->InitialParent = ParentMaterial;

	InstancePackage->FullyLoad();
	InstancePackage->Modify();

	ReportProgress(0.2f, FText::Format(
		               NSLOCTEXT("CastImporter", "MaterialFactoryCreate", "Factory Creating {0}"),
		               FText::FromString(FullMaterialName)));
	UMaterialInstanceConstant* MaterialInstance = Cast<UMaterialInstanceConstant>(
		MaterialInstanceFactory->FactoryCreateNew(
			UMaterialInstanceConstant::StaticClass(),
			InstancePackage,
			FName(*FullMaterialName),
			RF_Public | RF_Standalone | RF_Transactional,
			nullptr, GWarn
		)
	);

	if (!MaterialInstance)
	{
		UE_LOG(LogCast, Error, TEXT("MaterialInstanceFactory failed to create instance for %s."), *FullMaterialName);
		ReportProgress(1.0f, FText::Format(
			               NSLOCTEXT("CastImporter", "MaterialFailedFactory", "Material Failed (Factory) {0}"),
			               FText::FromString(FullMaterialName)));
		return nullptr;
	}

	FAssetRegistryModule::AssetCreated(MaterialInstance);

	// Apply Parameters
	MaterialInstance->Modify();
	MaterialInstance->PreEditChange(nullptr);

	ReportProgress(0.5f, FText::Format(
		               NSLOCTEXT("CastImporter", "MaterialSetParams", "Setting Parameters {0}"),
		               FText::FromString(FullMaterialName)));

	for (const FCastTextureInfo& TexInfo : MaterialInfo.Textures)
	{
		SetTextureParameter(MaterialInstance, TexInfo);
	}

	for (const FCastSettingInfo& Setting : MaterialInfo.Settings)
	{
		if (Setting.Name.IsEmpty()) continue;

		FMaterialParameterInfo ParameterInfo(FName(*Setting.Name), EMaterialParameterAssociation::GlobalParameter, -1);

		switch (Setting.Type)
		{
		case Float1:
			MaterialInstance->SetScalarParameterValueEditorOnly(ParameterInfo, Setting.Value.X);
			break;
		case Float2:
			MaterialInstance->SetVectorParameterValueEditorOnly(ParameterInfo,
			                                                    FLinearColor(
				                                                    Setting.Value.X, Setting.Value.Y, 0.0f, 0.0f));
			break;
		case Float3:
			MaterialInstance->SetVectorParameterValueEditorOnly(ParameterInfo,
			                                                    FLinearColor(
				                                                    Setting.Value.X, Setting.Value.Y, Setting.Value.Z,
				                                                    0.0f));
			break;
		case Color:
		case Float4:
			MaterialInstance->SetVectorParameterValueEditorOnly(ParameterInfo,
			                                                    FLinearColor(
				                                                    Setting.Value.X, Setting.Value.Y, Setting.Value.Z,
				                                                    Setting.Value.W));
			break;
		default:
			UE_LOG(LogCast, Warning, TEXT("Unhandled setting type %d for parameter %s in material %s"),
			       (int)Setting.Type, *Setting.Name, *FullMaterialName);
			break;
		}
	}

	Strategy->ApplyAdditionalParameters(MaterialInstance, MaterialInfo, bIsMetallic);

	// FStaticParameterSet StaticParameters;
	// MaterialInstance->GetStaticParameterValues(StaticParameters);
	// MaterialInstance->UpdateStaticPermutation(StaticParameters);

	MaterialInstance->PostEditChange();
	InstancePackage->MarkPackageDirty();

	ReportProgress(1.0f, FText::Format(
		               NSLOCTEXT("CastImporter", "MaterialCreated", "Created Material {0}"),
		               FText::FromString(FullMaterialName)));
	return MaterialInstance;
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
		FMaterialParameterInfo TextureParameterInfo(FName(*TextureInfo.TextureType),
		                                            EMaterialParameterAssociation::GlobalParameter, -1);
		Instance->SetTextureParameterValueEditorOnly(TextureParameterInfo, TextureAsset);
	}
	else
	{
		UE_LOG(LogCast, Warning, TEXT("TextureObject for parameter '%s' in material '%s' is not a UTexture."),
		       *TextureInfo.TextureType, *Instance->GetName());
	}
}

IMaterialCreationStrategy* FDefaultCastMaterialImporter::GetStrategy(ECastMaterialType Type) const
{
	const TSharedPtr<IMaterialCreationStrategy>* Found = MaterialStrategies.Find(Type);
	return Found ? Found->Get() : nullptr;
}
