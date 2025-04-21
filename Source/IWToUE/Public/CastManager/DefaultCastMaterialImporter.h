#pragma once

#include "CoreMinimal.h"
#include "Interface/ICastAssetImporter.h"
#include "Widgets/CastImportUI.h"

class FDefaultCastMaterialImporter : public ICastMaterialImporter
{
public:
	FDefaultCastMaterialImporter();
	virtual UMaterialInterface* CreateMaterialInstance(const FCastMaterialInfo& MaterialInfo,
	                                                   const FCastImportOptions& Options,
	                                                   UObject* ParentPackage) override;

	virtual void SetTextureParameter(UMaterialInstanceConstant* Instance, const FCastTextureInfo& TextureInfo) override;

private:
	IMaterialCreationStrategy* GetStrategy(ECastMaterialType Type) const;

	TMap<ECastMaterialType, TSharedPtr<IMaterialCreationStrategy>> MaterialStrategies;
};
