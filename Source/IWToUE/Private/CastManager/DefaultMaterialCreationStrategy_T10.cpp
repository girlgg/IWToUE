#include "CastManager/DefaultMaterialCreationStrategy_T10.h"

#include "CastManager/CastModel.h"
#include "Materials/MaterialInstanceConstant.h"

FString FDefaultMaterialCreationStrategy_T10::GetParentMaterialPath(const FCastMaterialInfo& MaterialInfo,
                                                                    bool& bOutIsMetallic) const
{
	FString MaterialType = TEXT("Base");
	bOutIsMetallic = false;

	for (const auto& Texture : MaterialInfo.Textures)
	{
		if (Texture.TextureObject && Texture.TextureType == TEXT("unk_semantic_0x57"))
		{
			if (Cast<UTexture2D>(Texture.TextureObject)->HasAlphaChannel())
			{
				bOutIsMetallic = true;
			}
		}
		// else if (Texture.TextureType == TEXT("unk_semantic_0x4D")) MaterialType = TEXT("Skin");
		// else if (Texture.TextureType == TEXT("unk_semantic_0x85")) MaterialType = TEXT("Hair");
		// else if (Texture.TextureType == TEXT("unk_semantic_0x86")) MaterialType = TEXT("Eye");
	}
	return FPaths::Combine(TEXT("/IWToUE/Shading/IW/T10"), TEXT("T10_") + MaterialType);
}

void FDefaultMaterialCreationStrategy_T10::ApplyAdditionalParameters(UMaterialInstanceConstant* MaterialInstance,
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
