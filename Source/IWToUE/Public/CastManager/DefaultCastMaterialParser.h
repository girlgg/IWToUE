#pragma once

#include "CastModel.h"
#include "Interface/ICastMaterialParser.h"

class FDefaultCastMaterialParser : public ICastMaterialParser
{
public:
	virtual bool ParseMaterialFiles(FCastMaterialInfo& MaterialInfo, const FString& MaterialBasePath,
	                                const FString& DefaultTexturePath, const FString& TextureFormat,
	                                ECastTextureImportType TexturePathType, const FString& GlobalMaterialPath) override;

private:
	bool ParseTextureLine(FCastTextureInfo& OutTexture, const FString& Line, const FString& BaseTexturePath,
	                      const FString& ImageFormat);
	bool ParseSettingLine(FCastSettingInfo& OutSetting, const FString& Line);
	FString ResolveTextureBasePath(const FString& MaterialName, const FString& DefaultTexturePath,
	                               ECastTextureImportType TexturePathType, const FString& GlobalMaterialPath);

	const TMap<FString, ESettingType> SettingTypeMap = {
		{TEXT("Color"), Color}, {TEXT("Float4"), Float4},
		{TEXT("Float3"), Float3}, {TEXT("Float2"), Float2},
		{TEXT("Float1"), Float1}
	};
};
