#pragma once
#include "WraithX/CoDAssetType.h"
#include "WraithX/GameProcess.h"

struct FCastModelInfo;
struct FWraithXModel;
struct FWraithXModelLod;
class FGameProcess;

namespace CoDBonesHelper
{
	using BoneIndexVariant = TVariant<TArray<uint8>, TArray<uint16>, TArray<uint32>>;

	void ReadXModelBones(TSharedPtr<IMemoryReader>& MemoryReader, const FWraithXModel& BaseModel,
	                     FWraithXModelLod& ModelLod, FCastModelInfo& ResultModel);

	void ReadBoneVariantInfo(TSharedPtr<IMemoryReader>& MemoryReader, const uint64& ReadAddr, uint64 BoneLen,
	                         int32 BoneIndexSize, BoneIndexVariant& OutBoneParentVariant);
};
