#pragma once

#include "CoreMinimal.h"

enum class ECastTextureImportType : uint8;
struct FCastMaterialInfo;

class ICastMaterialParser
{
public:
	virtual ~ICastMaterialParser() = default;
	virtual bool ParseMaterialFiles(FCastMaterialInfo& MaterialInfo, const FString& MaterialBasePath,
	                                const FString& DefaultTexturePath, const FString& TextureFormat,
	                                ECastTextureImportType TexturePathType, const FString& GlobalMaterialPath) = 0;
};
