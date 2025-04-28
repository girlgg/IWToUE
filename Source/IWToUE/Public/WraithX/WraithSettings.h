#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "WraithSettings.generated.h"

//----------------------------------------------------------------------
// Enums for Dropdown/Selector Options
//----------------------------------------------------------------------

UENUM(BlueprintType)
enum class EGameType : uint8
{
	UE4_Default UMETA(DisplayName = "Unreal Engine 4 (Default)"),
	CustomGameA UMETA(DisplayName = "Custom Game A"),
	CustomGameB UMETA(DisplayName = "Custom Game B (Example)"),
};

UENUM(BlueprintType)
enum class ETextureExportPathMode : uint8
{
	// "路径/贴图路径"
	RootAndTexture UMETA(DisplayName = "Export Path / Textures"),
	// "路径/模型路径/材质路径/贴图路径"
	ModelMaterialTexture UMETA(DisplayName = "Export Path / Model / Material / Textures"),
	// "路径/模型路径"
	ModelOnly UMETA(DisplayName = "Export Path / Model"),
	// "路径/材质路径"
	MaterialOnly UMETA(DisplayName = "Export Path / Material"),
};

UENUM(BlueprintType)
enum class EMaterialExportPathMode : uint8
{
	// "路径/材质路径"
	RootAndMaterial UMETA(DisplayName = "Export Path / Material"),
	// "路径/模型路径/材质路径"
	ModelAndMaterial UMETA(DisplayName = "Export Path / Model / Material"),
	// "路径/模型路径"
	ModelOnly UMETA(DisplayName = "Export Path / Model"),
};

//----------------------------------------------------------------------
// Settings Structs per Tab
//----------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FGeneralSettingsData
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, Category = "Loading", meta = (DisplayName = "Load Maps"))
	bool bLoadMaps = true;
	
	UPROPERTY(Config, EditAnywhere, Category = "Loading", meta = (DisplayName = "Load Models"))
	bool bLoadModels = true;

	UPROPERTY(Config, EditAnywhere, Category = "Loading", meta = (DisplayName = "Load Audio"))
	bool bLoadAudio = true;

	UPROPERTY(Config, EditAnywhere, Category = "Loading", meta = (DisplayName = "Load Materials"))
	bool bLoadMaterials = true;

	UPROPERTY(Config, EditAnywhere, Category = "Loading", meta = (DisplayName = "Load Animations"))
	bool bLoadAnimations = true;

	UPROPERTY(Config, EditAnywhere, Category = "Loading", meta = (DisplayName = "Load Textures"))
	bool bLoadTextures = true;

	UPROPERTY(Config, EditAnywhere, Category = "Paths", meta = (DisplayName = "Cordycep Tool Path"))
	FString CordycepToolPath;

	UPROPERTY(Config, EditAnywhere, Category = "Paths", meta = (DisplayName = "Game Path"))
	FString GamePath;

	UPROPERTY(Config, EditAnywhere, Category = "Game Configuration", meta = (DisplayName = "Game Type"))
	EGameType GameType = EGameType::UE4_Default;

	UPROPERTY(Config, EditAnywhere, Category = "Game Configuration", meta = (DisplayName = "Launch Parameters"))
	FString LaunchParameters;

	FGeneralSettingsData() = default;
};

USTRUCT(BlueprintType)
struct FModelSettingsData
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, Category = "Export", meta = (DisplayName = "Export Directory"))
	FString ExportDirectory = TEXT("Exports/Models");

	UPROPERTY(Config, EditAnywhere, Category = "Export", meta = (DisplayName = "Export LODs"))
	bool bExportLODs = true;

	UPROPERTY(Config, EditAnywhere, Category = "Export",
		meta = (DisplayName = "Max Export LOD Level", ClampMin = "0", UIMin = "0", EditCondition = "bExportLODs"))
	int32 MaxLODLevel = 3;

	UPROPERTY(Config, EditAnywhere, Category = "Export", meta = (DisplayName = "Export Vertex Colors"))
	bool bExportVertexColor = true;

	UPROPERTY(Config, EditAnywhere, Category = "Path Strategy", meta = (DisplayName = "Texture Path Mode"))
	ETextureExportPathMode TexturePathMode = ETextureExportPathMode::ModelMaterialTexture;

	UPROPERTY(Config, EditAnywhere, Category = "Path Strategy", meta = (DisplayName = "Material Path Mode"))
	EMaterialExportPathMode MaterialPathMode = EMaterialExportPathMode::ModelAndMaterial;

	FModelSettingsData() = default;
};

USTRUCT(BlueprintType)
struct FAnimationSettingsData
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, Category = "Export", meta = (DisplayName = "Export Directory"))
	FString ExportDirectory = TEXT("Exports/Animations");

	UPROPERTY(Config, EditAnywhere, Category = "Export",
		meta = (DisplayName = "Target Skeleton Asset Path", AllowedClasses = "/Script/Engine.Skeleton"))
	FString TargetSkeletonPath;

	FAnimationSettingsData() = default;
};

USTRUCT(BlueprintType)
struct FMaterialSettingsData
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, Category = "Paths", meta = (DisplayName = "Default Export Directory"))
	FString ExportDirectory = TEXT("Exports/Materials");

	UPROPERTY(Config, EditAnywhere, Category = "Paths", meta = (DisplayName = "Use Global Material Path"))
	bool bUseGlobalMaterialPath = false;

	UPROPERTY(Config, EditAnywhere, Category = "Paths",
		meta = (DisplayName = "Global Material Path", EditCondition = "bUseGlobalMaterialPath"))
	FString GlobalMaterialPath = TEXT("Exports/GlobalMaterials");

	FMaterialSettingsData() = default;
};

USTRUCT(BlueprintType)
struct FTextureSettingsData
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, Category = "Paths", meta = (DisplayName = "Default Export Directory"))
	FString ExportDirectory = TEXT("Exports/Textures");

	UPROPERTY(Config, EditAnywhere, Category = "Paths", meta = (DisplayName = "Use Global Texture Path"))
	bool bUseGlobalTexturePath = false;

	UPROPERTY(Config, EditAnywhere, Category = "Paths",
		meta = (DisplayName = "Global Texture Path", EditCondition = "bUseGlobalTexturePath"))
	FString GlobalTexturePath = TEXT("Exports/GlobalTextures");

	FTextureSettingsData() = default;
};

USTRUCT(BlueprintType)
struct FAudioSettingsData
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, Category = "Export", meta = (DisplayName = "Export Directory"))
	FString ExportDirectory = TEXT("Exports/Audio");

	FAudioSettingsData() = default;
};

USTRUCT(BlueprintType)
struct FMapSettingsData
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, Category = "Export", meta = (DisplayName = "Export Directory"))
	FString ExportDirectory = TEXT("Exports/Map");

	FMapSettingsData() = default;
};

//----------------------------------------------------------------------
// Main Settings UObject
//----------------------------------------------------------------------

UCLASS(Config = EditorPerProjectUserSettings, DefaultConfig)
class IWTOUE_API UWraithSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Settings")
	FGeneralSettingsData General;

	UPROPERTY(Config, EditAnywhere, Category = "Settings")
	FModelSettingsData Model;

	UPROPERTY(Config, EditAnywhere, Category = "Settings")
	FAnimationSettingsData Animation;

	UPROPERTY(Config, EditAnywhere, Category = "Settings")
	FMaterialSettingsData Material;

	UPROPERTY(Config, EditAnywhere, Category = "Settings")
	FTextureSettingsData Texture;

	UPROPERTY(Config, EditAnywhere, Category = "Settings")
	FAudioSettingsData Audio;

	UPROPERTY(Config, EditAnywhere, Category = "Settings")
	FMapSettingsData Map;

	DECLARE_MULTICAST_DELEGATE(FOnSettingsChanged);
	static FOnSettingsChanged OnSettingsChanged;
	
	void Save();
	
	static FText GetGameTypeDisplayName(EGameType Value);
	static FText GetTextureExportPathModeDisplayName(ETextureExportPathMode Value);
	static FText GetMaterialExportPathModeDisplayName(EMaterialExportPathMode Value);
};
