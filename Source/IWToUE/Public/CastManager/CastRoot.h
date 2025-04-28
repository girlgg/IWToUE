#pragma once

struct FCastMaterialInfo;
struct FCastModelLod;
struct FCastAnimationInfo;
struct FCastModelInfo;

struct FCastInstanceInfo
{
	FString Name;
	uint64 ReferenceFileHash;
	FVector3f Position;
	FVector4f Rotation;
	FVector3f Scale;
};

struct FCastMetadataInfo
{
	FString Author;
	FString Software;
	FString UpAxis;
};

struct FCastRoot
{
public:
	TArray<FCastModelInfo> Models;
	TArray<FCastModelLod> ModelLodInfo;
	TArray<FCastAnimationInfo> Animations;
	TArray<FCastInstanceInfo> Instances;
	TArray<FCastMetadataInfo> Metadata;

	TArray<FCastMaterialInfo> Materials;
	// 哈希对应以上的index
	TMap<uint64, uint32> MaterialMap;
};

