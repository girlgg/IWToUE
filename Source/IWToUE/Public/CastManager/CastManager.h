#pragma once

#include "CastScene.h"
#include "Serialization/LargeMemoryReader.h"

struct FCastAnimationInfo;
struct FCastMaterialInfo;
struct FCastSkeletonInfo;
struct FCastModelInfo;

#define COPY_ARR(src_arr, dst_arr) for (const auto& Val : src_arr) dst_arr.Add(Val);

class FCastManager
{
public:
	~FCastManager();

	bool Initialize(FString InFilePath);
	bool Import();
	void ReleaseScene();

	void Destroy();

	TSharedPtr<FCastScene> Scene{nullptr};

	int32 GetBoneNum() const;
	int32 GetVertexNum() const;
	int32 GetFaceNum() const;

protected:
	int32 GetNode();

	void ProcessCastData(FCastNode& Node);
	void ProcessRootData(FCastNode& Node, FCastRoot& Root);
	void ProcessModelData(FCastNode& Node, FCastRoot& Root,FCastModelInfo& Model);
	void ProcessSkeletonData(FCastNode& Node, FCastSkeletonInfo& Skeleton);
	void ProcessMaterialData(FCastNode& Node, FCastMaterialInfo& Material);
	void ProcessAnimationData(FCastNode& Node, FCastAnimationInfo& Animation);

private:
	TArray64<uint8> FileDataOld;
	TUniquePtr<FLargeMemoryReader> Reader;

	FString FilePath;

	bool bWasDestroy{false};
};
