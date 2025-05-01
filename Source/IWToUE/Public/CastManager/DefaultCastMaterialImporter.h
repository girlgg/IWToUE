#pragma once

#include "CoreMinimal.h"
#include "CastModel.h"
#include "Interface/ICastAssetImporter.h"
#include "Widgets/CastImportUI.h"

struct FPreparedMaterialInstanceData
{
	bool bIsValid = false;
	FString SanitizedMaterialName;
	FString FinalPackageName;
	FString ParentMaterialPath;
	bool bParentMaterialIsMetallic = false;

	TArray<TTuple<FName, UObject*>> TextureParametersToSet;
	TArray<TTuple<FName, float>> ScalarParametersToSet;
	TArray<TTuple<FName, FLinearColor>> VectorParametersToSet;

	FCastMaterialInfo OriginalMaterialInfo;
	const IMaterialCreationStrategy* Strategy = nullptr;
};

class FDefaultCastMaterialImporter : public ICastMaterialImporter
{
public:
	FDefaultCastMaterialImporter();
	virtual UMaterialInterface* CreateMaterialInstance(const FCastMaterialInfo& MaterialInfo,
	                                                   const FCastImportOptions& Options,
	                                                   UObject* ParentPackage) override;

	virtual void SetTextureParameter(UMaterialInstanceConstant* Instance, const FCastTextureInfo& TextureInfo) override;

protected:
	FPreparedMaterialInstanceData PrepareMaterialInstanceData_OffThread(
		const FCastMaterialInfo& MaterialInfo,
		const FCastImportOptions& Options,
		UObject* ParentPackage);

	UMaterialInstanceConstant* ApplyMaterialInstanceData_GameThread(
		FPreparedMaterialInstanceData& PreparedData);

private:
	IMaterialCreationStrategy* GetStrategy(ECastMaterialType Type) const;

	TMap<ECastMaterialType, TSharedPtr<IMaterialCreationStrategy>> MaterialStrategies;
};
