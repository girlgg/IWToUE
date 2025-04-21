#include "CastManager/DefaultMaterialCreationStrategy_IW9.h"

#include "CastManager/CastModel.h"
#include "Materials/MaterialInstanceConstant.h"

FString FDefaultMaterialCreationStrategy_IW9::GetParentMaterialPath(const FCastMaterialInfo& MaterialInfo,
                                                                    bool& bOutIsMetallic) const
{
	FString MaterialType = TEXT("Base");
	bOutIsMetallic = false;

	for (const auto& Texture : MaterialInfo.Textures)
	{
		if (Texture.TextureObject && Texture.TextureType == TEXT("unk_semantic_0x0"))
		{
			if (Cast<UTexture2D>(Texture.TextureObject)->HasAlphaChannel())
			{
				bOutIsMetallic = true;
			}
		}
		else if (Texture.TextureType == TEXT("unk_semantic_0x7F")) MaterialType = TEXT("Skin");
		else if (Texture.TextureType == TEXT("unk_semantic_0x85")) MaterialType = TEXT("Hair");
		else if (Texture.TextureType == TEXT("unk_semantic_0x86")) MaterialType = TEXT("Eye");
	}
	return FPaths::Combine(TEXT("/IWToUE/Shading/IW/IW9"), TEXT("IW9_") + MaterialType);
}

void FDefaultMaterialCreationStrategy_IW9::ApplyAdditionalParameters(
	UMaterialInstanceConstant* MaterialInstance,
	const FCastMaterialInfo& MaterialInfo,
	bool bIsMetallic) const
{
	if (bIsMetallic)
	{
		FMaterialParameterInfo ParameterInfo(FName("Default Spec"), GlobalParameter, -1);
		MaterialInstance->SetScalarParameterValueEditorOnly(ParameterInfo, 0.0f);
		UE_LOG(LogCast, Log, TEXT("Applied metallic override for material %s"), *MaterialInstance->GetName());
	}
}
