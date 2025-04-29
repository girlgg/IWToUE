#pragma once
#include "CastScene.h"

struct FCastBlendShape
{
	FString Name;
	uint64 MeshHash;
	TArray<uint32> TargetShapeVertexIndices;
	TArray<FVector3f> TargetShapeVertexPositions;
	TArray<float> TargetWeightScale;
};

struct FCastBoneInfo
{
	FString BoneName{""};
	int32 ParentIndex{-1};
	bool SegmentScaleCompensate{false};
	bool bIsCosmetic;
	FVector3f LocalPosition{};
	FVector4f LocalRotation{0, 0, 0, 1};
	FVector3f WorldPosition{};
	FVector4f WorldRotation{0, 0, 0, 1};
	FVector3f Scale{1.f, 1.f, 1.f};
};

struct FCastIKHandle
{
	FString Name;
	uint64 StartBoneHash;
	uint64 EndBoneHash;
	uint64 TargetBoneHash;
	uint64 PoleVectorBoneHash;
	uint64 PoleBoneHash;
	bool UseTargetRotation;
};

struct FCastConstraint
{
	FString Name;
	FString ConstraintType;
	uint64 ConstraintBoneHash;
	uint64 TargetBoneHash;
	bool MaintainOffset;
	bool SkipX;
	bool SkipY;
	bool SkipZ;
};

struct FCastSkeletonInfo
{
	TArray<FCastBoneInfo> Bones;
	TArray<FCastIKHandle> IKHandles;
	TArray<FCastConstraint> Constraints;
};

class FCastTextureInfo
{
public:
	UTexture2D* TextureObject{nullptr};
	FString TextureSemantic{""};
	FString TextureName{""};
	FString TexturePath{""};
	FString TextureType{""};
	FString TextureSlot{""};
};

enum ESettingType : int
{
	Color UMETA(DisplayName = "Color"),
	Float4 UMETA(DisplayName = "Float4"),
	Float3 UMETA(DisplayName = "Float3"),
	Float2 UMETA(DisplayName = "Float2"),
	Float1 UMETA(DisplayName = "Float1"),
};

class FCastSettingInfo
{
public:
	ESettingType Type{Float4};
	FString Name{""};
	FVector4 Value{0};
};

struct FCastMaterialInfo
{
	uint64 MaterialHash = 0;
	uint64 MaterialPtr = 0;
	FString Name;
	FString TechSet;
	FString Type;
	uint64 AlbedoFileHash = 0;
	uint64 DiffuseFileHash = 0;
	uint64 NormalFileHash = 0;
	uint64 SpecularFileHash = 0;
	uint64 EmissiveFileHash = 0;
	uint64 GlossFileHash = 0;
	uint64 RoughnessFileHash = 0;
	uint64 AmbientOcclusionFileHash = 0;
	uint64 CavityFileHash = 0;
	uint64 AnisotropyFileHash = 0;
	uint64 ExtraFileHash = 0;

	UMaterialInterface* MaterialObject{nullptr};

	TMap<uint64, FString> FileMap;

	// images.txt
	TArray<FCastTextureInfo> Textures;

	// settings.txt
	TArray<FCastSettingInfo> Settings;

public:
	FORCEINLINE FString GetName() const { return Name; }
	void SetName(const FString& InName) { Name = InName; }
};

struct FCastModelInfo
{
	FString Name;
	TArray<FCastMeshInfo> Meshes;
	TArray<FCastSkeletonInfo> Skeletons;
	TArray<FCastBlendShape> BlendShapes;

	int32 GetTotalVertexCount() const
	{
		int32 TotalVertices = 0;
		for (const FCastMeshInfo& Mesh : Meshes)
		{
			TotalVertices += Mesh.GetVertexCount();
		}
		return TotalVertices;
	}
};

struct FCastModelLod
{
	float Distance{0.f};
	float MaxDistance{0.f};
	int32 ModelIndex{-1};
};
