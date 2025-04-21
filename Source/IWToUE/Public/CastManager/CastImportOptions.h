#pragma once
#include "Widgets/CastImportUI.h"

struct FCastImportOptions
{
	TObjectPtr<USkeleton> Skeleton = nullptr;
	bool bImportAsSkeletal = true;
	bool bImportMesh = true;
	bool bReverseFace = false;
	bool bPhysicsAsset{true};
	TObjectPtr<UPhysicsAsset> PhysicsAsset = nullptr;
	bool bImportAnimations = false;
	bool bImportAnimationNotify = false;
	bool bDeleteRootNodeAnim = false;
	bool bConvertRefPosition = false;
	bool bConvertRefAnim = false;
	bool bImportMaterial = true;
	ECastMaterialType MaterialType = ECastMaterialType::CastMT_IW9;
	ECastTextureImportType TexturePathType = ECastTextureImportType::CastTIT_Default;
	FString GlobalMaterialPath;
	FString TextureFormat = TEXT("png");
};
