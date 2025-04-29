#pragma once

#include "CoreMinimal.h"
#include "Structures/SharedStructures.h"
#include "UObject/Object.h"

#define LOCTEXT_NAMESPACE "CodAssetType"

enum class EWraithAssetType : uint8
{
	// An animation asset
	Animation,
	// An image asset
	Image,
	// A model asset
	Model,
	// A sound file asset
	Sound,
	// A fx file asset
	Effect,
	// A raw file asset
	RawFile,
	// A raw file asset
	Material,
	// A custom asset
	Custom,
	Map,
	// An unknown asset, not loaded
	Unknown,
};

enum class EWraithAssetStatus : uint8
{
	// The asset is currently loaded
	Loaded,
	// The asset was exported
	Exported,
	// The asset was not loaded
	NotLoaded,
	// The asset is a placeholder
	Placeholder,
	// The asset is being processed
	Processing,
	// The asset had an error
	Error
};

struct FWraithAsset : public TSharedFromThis<FWraithAsset>
{
	virtual ~FWraithAsset() = default;

	// The type of asset we have
	EWraithAssetType AssetType{EWraithAssetType::Unknown};
	// The name of this asset
	FString AssetName;
	// The status of this asset
	EWraithAssetStatus AssetStatus{EWraithAssetStatus::NotLoaded};
	// The size of this asset, -1 = N/A
	int64 AssetSize{-1};
	// Whether or not the asset is streamed
	bool Streamed{false};

	FText GetAssetStatusText() const
	{
		TArray AssetStatusText = {
			LOCTEXT("AssetStatusLoaded", "Loaded"),
			LOCTEXT("AssetStatusExported", "Exported"),
			LOCTEXT("AssetStatusNotLoaded", "NotLoaded"),
			LOCTEXT("AssetStatusPlaceholder", "Placeholder"),
			LOCTEXT("AssetStatusProcessing", "Processing"),
			LOCTEXT("AssetStatusError", "Error")
		};
		return AssetStatusText[static_cast<uint8>(AssetStatus)];
	}

	virtual FText GetAssetTypeText() const
	{
		return LOCTEXT("AssetType", "Unknown");
	}
};

struct FCoDAsset : FWraithAsset
{
	// Whether or not this is a file entry
	bool bIsFileEntry = false;

	// A pointer to the asset in memory
	uint64 AssetPointer = 0;

	// The assets offset in the loaded pool
	uint32 AssetLoadedIndex = 0;
};

struct FCoDModel : FCoDAsset
{
	// The bones
	TArray<FString> BoneNames{};
	// The bone count
	uint32 BoneCount{};
	// The cosmetic bone count
	uint32 CosmeticBoneCount{};
	// The lod count
	uint16 LodCount{};

	virtual FText GetAssetTypeText() const
	{
		return LOCTEXT("AssetType", "Model");
	}
};

struct FCoDMap : FCoDAsset
{
	virtual FText GetAssetTypeText() const
	{
		return LOCTEXT("AssetType", "Map");
	}
};

struct FCoDImage : FCoDAsset
{
	// The width
	uint16 Width{};
	// The height
	uint16 Height{};
	// The format
	uint16 Format{};

	virtual FText GetAssetTypeText() const
	{
		return LOCTEXT("AssetType", "Image");
	}
};

struct FCoDAnim : FCoDAsset
{
	// The bones
	std::vector<std::string> BoneNames;
	// The framerate
	float Framerate;
	// The number of frames
	uint32 FrameCount;
	// The number of frames
	uint32 BoneCount;
	// The number of shapes
	uint32 ShapeCount;

	// Modern games
	bool isSiegeAnim;

	virtual FText GetAssetTypeText() const
	{
		return LOCTEXT("AssetType", "Animation");
	}
};

struct FCoDMaterial : FCoDAsset
{
	int32 ImageCount;

	virtual FText GetAssetTypeText() const
	{
		return LOCTEXT("AssetType", "Material");
	}
};

enum class FCoDSoundDataTypes : uint8
{
	WAV_WithHeader,
	WAV_NeedsHeader,
	FLAC_WithHeader,
	FLAC_NeedsHeader,
	Opus_Interleaved,
	Opus_Interleaved_Streamed,
};

struct FCoDSound : public FCoDAsset
{
	// The full file path of the audio file
	FString FullName;
	// The full file path of the audio file
	FString FullPath;
	// The framerate of this audio file
	uint32 FrameRate;
	// The total frame count
	uint32 FrameCount;
	// Channels count
	uint8 ChannelsCount;
	// Channels count
	uint32 Length;
	// The index of the sound package for this entry
	uint16 PackageIndex;
	// Whether or not this is a localized entry
	bool IsLocalized;

	// The datatype for this entry
	FCoDSoundDataTypes DataType;

	virtual FText GetAssetTypeText() const
	{
		return LOCTEXT("AssetType", "Sound");
	}
};

struct FWraithXModelSubmesh
{
	uint64 MaterialHash;
	// Count of rigid vertex pairs
	uint32 VertListcount = 0;
	// A pointer to rigid weights
	uint64 RigidWeightsPtr = 0;

	// Count of verticies
	uint32 VertexCount = 0;
	// Count of faces
	uint32 FaceCount = 0;
	// Count of packed index tables
	uint32 PackedIndexTableCount = 0;

	// Pointer to faces
	uint64 FacesOffset = 0;
	// Pointer to verticies
	uint64 VertexOffset = 0;
	// Pointer to verticies
	uint64 VertexTangentOffset = 0;
	// Pointer to UVs
	uint64 VertexUVsOffset = 0;
	// Pointer to vertex colors
	uint64 VertexColorOffset = 0;
	// Pointer to packed index table
	uint64 PackedIndexTableOffset = 0;
	// Pointer to packed index buffer
	uint64 PackedIndexBufferOffset = 0;

	// A list of weights
	uint16 WeightCounts[8] = {};
	// A pointer to weight data
	uint64 WeightsOffset = 0;
	// A pointer to blendshapes data
	uint64 BlendShapesPtr = 0;

	// Submesh Scale
	float Scale = 1;

	// Submesh Offset
	float XOffset = 0;
	float YOffset = 0;
	float ZOffset = 0;

	// The index of the material
	int32 MaterialIndex = -1;
};

struct FWraithXMaterialSetting
{
	// The name
	FString Name;
	// The type
	FString Type;
	// The data
	float Data[4];
};

enum class EImageUsageType : uint8
{
	Unknown,
	DiffuseMap,
	NormalMap,
	SpecularMap,
	GlossMap
};

struct FWraithXImage
{
	// The usage of this image asset
	EImageUsageType ImageUsage;
	// The pointer to the image asset
	uint64 ImagePtr;

	uint32 SemanticHash;

	// The name of this image asset
	FString ImageName;

	UTexture2D* ImageObject = nullptr;
};

struct FWraithXMaterial
{
	uint64 MaterialHash;
	uint64 MaterialPtr;
	// The material name
	FString MaterialName;
	// The techset name
	FString TechsetName;
	// The surface type name
	FString SurfaceTypeName;

	// A list of images
	TArray<FWraithXImage> Images;

	// A list of settings, if any
	TArray<FWraithXMaterialSetting> Settings;
};

struct FWraithXModelLod
{
	// Name of the LOD
	FString Name;
	// A list of submeshes for this specific lod
	TArray<FWraithXModelSubmesh> Submeshes;
	// A list of material info per-submesh
	TArray<FWraithXMaterial> Materials;

	// A key used for the lod data if it's streamed
	uint64 LODStreamKey;
	// A pointer used for the stream mesh info
	uint64 LODStreamInfoPtr;

	// The distance this lod displays at
	float LodDistance;
	// The max distance this lod displays at
	float LodMaxDistance;
};

enum class EBoneDataTypes
{
	DivideBySize,
	QuatPackingA,
	HalfFloat
};

struct FWraithXModel
{
	// The name of the asset
	FString ModelName;

	// The type of data used for bone rotations
	EBoneDataTypes BoneRotationData;

	// Whether or not we use the stream mesh loader
	bool IsModelStreamed;

	// The model bone count
	uint32 BoneCount = 0;
	// The model root bone count
	uint32 RootBoneCount = 0;
	// The model cosmetic bone count
	uint32 CosmeticBoneCount = 0;
	// The model blendshape count
	uint32 BlendShapeCount = 0;

	// A pointer to bone name string indicies
	uint64 BoneIDsPtr = 0;
	// The size of the bone name index
	uint8 BoneIndexSize = 0;

	// A pointer to bone parent indicies
	uint64 BoneParentsPtr = 0;
	// The size of the bone parent index
	uint8 BoneParentSize = 0;

	// A pointer to local rotations
	uint64 RotationsPtr = 0;
	// A pointer to local positions
	uint64 TranslationsPtr = 0;

	// A pointer to global matricies
	uint64 BaseTransformPtr = 0;

	// A pointer to the bone collision data, hitbox offsets
	uint64 BoneInfoPtr = 0;

	// A pointer to the blendshape names
	uint64 BlendShapeNamesPtr = 0;

	// A list of lods per this model
	TArray<FWraithXModelLod> ModelLods;
};
struct FWraithMapMeshData
{
	FString MeshName;
	FCastMeshInfo MeshData;
};
struct FWraithMapStaticModelInstance
{
	uint64 ModelAssetPtr;
	FString InstanceName;
	FTransform Transform;
};
struct FWraithXMap
{
	FString MapName;
	TArray<FWraithMapMeshData> MapMeshes;
	TArray<FCastMaterialInfo> MapMeshMaterials;
	TMap<uint64, uint32> MapMeshMaterialMap;
	TArray<FWraithMapStaticModelInstance> StaticModelInstances;
};

struct FMW6XAnimStreamInfo
{
	uint64 StreamKey;
	uint32 Size;
	uint32 Padding;
};

enum class EAnimationKeyTypes
{
	DivideBySize,
	MinSizeTable,
	QuatPackingA,
	HalfFloat
};

struct FMW6XAnimNoteTrack
{
	uint32 Name;
	float Time;
	uint8 Padding[24];
};

struct FWraithXAnim
{
	// The name of the asset
	FString AnimationName;

	// The framerate of this animation
	float FrameRate;
	// The framecount of this animation
	uint32 FrameCount;

	// Whether or not this is a viewmodel animation
	bool ViewModelAnimation = false;
	// Whether or not this animation loops
	bool LoopingAnimation = false;
	// Whether or not this animation is additive
	bool AdditiveAnimation = false;
	uint8 AnimType;
	// Whether or not we support inline-indicies
	bool SupportsInlineIndicies;

	// A pointer to bone name string indicies
	uint64 BoneIDsPtr;
	// The size of the bone name index
	uint8 BoneIndexSize;
	// The size of the inline bone indicies (0) when not in use
	uint8 BoneTypeSize;

	// What type of rotation data we have
	EAnimationKeyTypes RotationType;
	// What type of translation data we have
	EAnimationKeyTypes TranslationType;

	// A pointer to animation byte size data
	uint64 DataBytesPtr;
	// A pointer to animation short size data
	uint64 DataShortsPtr;
	// A pointer to animation integer size data
	uint64 DataIntsPtr;

	// A pointer to animation (rand) byte size data
	uint64 RandomDataBytesPtr;
	// A pointer to animation (rand) short size data
	uint64 RandomDataShortsPtr;
	// A pointer to animation (rand) integer size data
	uint64 RandomDataIntsPtr;

	// A pointer to animation indicies (When we have more than 255)
	uint64 LongIndiciesPtr;
	// A pointer to animation notetracks
	uint64 NotificationsPtr;
	// A pointer to blendshape names
	uint64 BlendShapeNamesPtr;
	// A pointer to blendshape weights
	uint64 BlendShapeWeightsPtr;

	// A pointer to animation delta translations
	uint64 DeltaTranslationPtr;
	// A pointer to 2D animation delta rotations
	uint64 Delta2DRotationsPtr;
	// A pointer to 3D animation delta rotations
	uint64 Delta3DRotationsPtr;

	// The count of non-rotated bones
	uint32 NoneRotatedBoneCount;
	// The count of 2D rotated bones
	uint32 TwoDRotatedBoneCount;
	// The count of 3D rotated bones
	uint32 NormalRotatedBoneCount;
	// The count of 2D static rotated bones
	uint32 TwoDStaticRotatedBoneCount;
	// The count of 3D static rotated bones
	uint32 NormalStaticRotatedBoneCount;
	// The count of normal translated bones
	uint32 NormalTranslatedBoneCount;
	// The count of precise translated bones
	uint32 PreciseTranslatedBoneCount;
	// The count of static translated bones
	uint32 StaticTranslatedBoneCount;
	// The count of non-translated bones
	uint32 NoneTranslatedBoneCount;
	// The total bone count
	uint32 TotalBoneCount;
	// The count of notetracks
	uint32 NotificationCount;
	// The count of blendshape weights
	uint32 BlendShapeWeightCount;

	// XAnim Reader (Streamed)
	FCoDXAnimReader Reader;
	// XAnim Reader Function
	// std::function<void(const std::unique_ptr<XAnim_t>&, std::unique_ptr<WraithAnim>&)> ReaderFunction;
	// XAnim Reader Information Pointer.
	uint64 ReaderInformationPointer;
};

// 骨骼位置信息
struct FCastBoneTransform
{
	FVector4f Rotation; // 16 bytes (x, y, z, w)
	FVector3f Translation; // 12 bytes (x, y, z)
	float TranslationWeight; // 4 bytes
};

enum class ESoundDataTypes : uint8
{
	Wav_WithHeader,
	Wav_NeedsHeader,
	Flac_WithHeader,
	Flac_NeedsHeader,
	Opus_Interleaved,
	Opus_Interleaved_Streamed,
};

struct FWraithXSound
{
	TArray<uint8> Data;
	uint32 DataSize;
	ESoundDataTypes DataType;
	uint32 FrameCount;
	uint32 FrameRate;
	uint8 ChannelCount;
};

#undef LOCTEXT_NAMESPACE
