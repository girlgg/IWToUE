#pragma once

struct FCastRoot;
class FCastNode;

struct FCastWeightsData
{
	// The weight value for each bone
	float WeightValues[8];
	// The bone ids for each value
	uint32_t BoneValues[8];
	// The count of weights this set contains
	uint8_t WeightCount;

	FCastWeightsData()
	{
		// Defaults
		WeightCount = 1;
		// Clear memory
		WeightValues[0] = 1.0f;
		WeightValues[1] = 1.0f;
		WeightValues[2] = 1.0f;
		WeightValues[3] = 1.0f;
		WeightValues[4] = 1.0f;
		WeightValues[5] = 1.0f;
		WeightValues[6] = 1.0f;
		WeightValues[7] = 1.0f;
		BoneValues[0] = 0;
		BoneValues[1] = 0;
		BoneValues[2] = 0;
		BoneValues[3] = 0;
		BoneValues[4] = 0;
		BoneValues[5] = 0;
		BoneValues[6] = 0;
		BoneValues[7] = 0;
	}
};

struct FCastMeshInfo
{
	FString Name{""};
	TArray<FVector3f> VertexPositions{};
	TArray<FVector3f> VertexNormals{};
	TArray<FVector3f> VertexTangents{};
	TArray<uint32> VertexColor{};
	TArray<FVector2f> VertexUV{};
	TArray<FCastWeightsData> VertexWeights;
	TArray<uint32> VertexWeightBone{};
	TArray<float> VertexWeightValue{};
	TArray<uint32> Faces{};
	TArray<uint32> ColorLayer{};
	uint32 UVLayer{};
	uint32 MaxWeight{}; // 每个顶点被多少个骨骼影响权重
	FString SkinningMethod{};
	uint64 MaterialHash = 0;
	uint64 MaterialPtr = 0;
	int32 MaterialIndex = 0;

	FVector BBoxMax{0};
	FVector BBoxMin{0};

	void ComputeBBox()
	{
		for (const FVector3f& Pos : VertexPositions)
		{
			BBoxMax.X = FMath::Max(BBoxMax.X, Pos.X);
			BBoxMax.Y = FMath::Max(BBoxMax.Y, Pos.Y);
			BBoxMax.Z = FMath::Max(BBoxMax.Z, Pos.Z);

			BBoxMin.X = FMath::Min(BBoxMin.X, Pos.X);
			BBoxMin.Y = FMath::Min(BBoxMin.Y, Pos.Y);
			BBoxMin.Z = FMath::Min(BBoxMin.Z, Pos.Z);
		}
	}

	int32 GetVertexCount() const
	{
		return VertexPositions.Num();
	}
};

struct FCastNodeInfo
{
};

struct FCastSceneInfo
{
	int32 NonSkinnedMeshNum;

	int32 SkinnedMeshNum;

	int32 TotalGeometryNum;
	int32 TotalMaterialNum;
	int32 TotalTextureNum;

	TArray<FCastMeshInfo> MeshInfo;
	TArray<FCastNodeInfo> HierarchyInfo;

	bool bHasAnimation;
	double FrameRate;
	double TotalTime;

	void Reset()
	{
		NonSkinnedMeshNum = 0;
		SkinnedMeshNum = 0;
		TotalGeometryNum = 0;
		TotalMaterialNum = 0;
		TotalTextureNum = 0;
		MeshInfo.Empty();
		HierarchyInfo.Empty();
		bHasAnimation = false;
		FrameRate = 0.0;
		TotalTime = 0.0;
	}
};

struct FCastHeader
{
	uint32 Magic; // char[4] cast	(0x74736163)
	uint32 Version; // 0x1
	uint32 RootNodes; // Number of root nodes, which contain various sub nodes if necessary
	uint32 Flags; // Reserved for flags, or padding, whichever is needed
};

struct FCastScene
{
public:
	int32 GetNodeCount() const;
	int32 GetMaterialCount() const;
	int32 GetTextureCount() const;
	int32 GetSkinnedMeshNum() const;
	int32 GetMeshNum() const;

	bool HasAnimation() const;
	float GetAnimFramerate() const;

	FCastNode* GetNode(int32 Idx);

	FCastHeader Header{};

	TArray<int32> RootNodes;
	TArray<FCastNode> Nodes;

	TArray<FCastRoot> Roots;

private:
};
