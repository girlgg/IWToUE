#pragma once

#include "CoreMinimal.h"
#include "Interface/ICastAssetImporter.h"

class FDefaultCastTextureImporter: public ICastTextureImporter
{
public:
	virtual UTexture2D* ImportOrLoadTexture(FCastTextureInfo& TextureInfo, const FString& AbsoluteTextureFilePath,
	                                        const FString& AssetDestinationPath) override;
};
