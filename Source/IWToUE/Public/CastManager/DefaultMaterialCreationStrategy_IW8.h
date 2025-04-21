#pragma once

#include "CoreMinimal.h"
#include "Interface/ICastAssetImporter.h"

class FDefaultMaterialCreationStrategy_IW8 : public IMaterialCreationStrategy
{
public:
	virtual FString GetParentMaterialPath(const FCastMaterialInfo& MaterialInfo, bool& bOutIsMetallic) const override;
	virtual void ApplyAdditionalParameters(UMaterialInstanceConstant* MaterialInstance,
	                                       const FCastMaterialInfo& MaterialInfo, bool bIsMetallic) const override;
};
