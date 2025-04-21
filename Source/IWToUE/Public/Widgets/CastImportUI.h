#pragma once

#include "CastImportUI.generated.h"

UENUM(BlueprintType)
enum class ECastImportType : uint8
{
	Cast_StaticMesh UMETA(DisplayName="Static Mesh"),
	Cast_SkeletalMesh UMETA(DisplayName="Skeletal Mesh"),
	Cast_Animation UMETA(DisplayName="Animation"),

	Cast_Max,
};

UENUM(BlueprintType)
enum class ECastMaterialType : uint8
{
	// CastMT_T7 UMETA(DisplayName = "T7 Engine"),
	CastMT_IW8 UMETA(DisplayName = "IW8 Engine"),
	CastMT_IW9 UMETA(DisplayName = "IW9/JUP Engine"),
	CastMT_T10 UMETA(DisplayName = "T10 Engine")
};

UENUM(BlueprintType)
enum class ECastAnimImportType : uint8
{
	CastAIT_Auto UMETA(DisplayName="Automatically detect"),
	CastAIT_Absolutely UMETA(DisplayName="Force as absolute"),
	CastAIT_Relative UMETA(DisplayName="Force as relative")
};

UENUM(BlueprintType)
enum class ECastTextureImportType : uint8
{
	CastTIT_Default UMETA(DisplayName="Model/Material/Image"),
	CastTIT_GlobalMaterials UMETA(DisplayName="GlobalMaterials/Material/Image"),
	CastTIT_GlobalImages UMETA(DisplayName="GlobalImages/Image")
};

UCLASS(BlueprintType, AutoExpandCategories=(FTransform), HideCategories=Object, MinimalAPI,
	Config = EditorPerProjectUserSettings)
class UCastImportUI : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Mesh,
		meta = (EditCondition = "bImportAsSkeletal || bImportAnimations", AllowedClasses = "/Script/Engine.Skeleton"))
	TObjectPtr<USkeleton> Skeleton = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Mesh,
		meta = (DisplayName="As Skeletal Mesh", EditCondition = "bImportMesh"))
	bool bImportAsSkeletal = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Mesh)
	bool bImportMesh = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Mesh, meta=(EditCondition="bImportMesh"))
	bool bReverseFace = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category=Mesh,
		meta=(EditCondition = "bImportAsSkeletal"))
	bool bPhysicsAsset{true};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Mesh,
		meta = (EditCondition = "bPhysicsAsset", AllowedClasses = "/Script/Engine.PhysicsAsset"))
	TObjectPtr<UPhysicsAsset> PhysicsAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Animation)
	bool bImportAnimations = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Animation,
		meta=(EditCondition="bImportAnimations"))
	bool bImportAnimationNotify = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Animation,
		meta=(EditCondition="bImportAnimations"))
	bool bDeleteRootNodeAnim = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Animation,
		meta=(EditCondition="bImportAnimations"))
	bool bConvertRefPosition = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Animation,
		meta=(EditCondition="bImportAnimations"))
	bool bConvertRefAnim = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Material)
	bool bImportMaterial = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Material, meta=(EditCondition="bImportMaterial"))
	ECastMaterialType MaterialType = ECastMaterialType::CastMT_IW9;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Material, meta=(EditCondition="bImportMaterial"))
	ECastTextureImportType TexturePathType = ECastTextureImportType::CastTIT_Default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Material,
		meta = (EditCondition = "TexturePathType != ECastTextureImportType::CastTIT_Default"))
	FString GlobalMaterialPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Material)
	FString TextureFormat = TEXT("png");
};
