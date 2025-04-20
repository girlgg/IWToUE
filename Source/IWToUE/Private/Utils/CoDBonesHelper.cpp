#include "Utils/CoDBonesHelper.h"

#include "CastManager/CastModel.h"
#include "Interface/IMemoryReader.h"
#include "WraithX/GameProcess.h"

void CoDBonesHelper::ReadXModelBones(TSharedPtr<IMemoryReader>& MemoryReader, const FWraithXModel& BaseModel,
                                     FWraithXModelLod& ModelLod, FCastModelInfo& ResultModel)
{
	// 获取骨骼信息
	uint32 BoneCount = BaseModel.BoneCount + BaseModel.CosmeticBoneCount;

	// TODO 加载不同模型骨骼
	if (!ResultModel.Skeletons.IsEmpty())
	{
		return;
	}

	FCastSkeletonInfo& SkeletonInfo = ResultModel.Skeletons.AddDefaulted_GetRef();

	if (BoneCount <= 0)
	{
		return;
	}

	// 预处理
	uint64 ChildBoneSize = BoneCount - BaseModel.RootBoneCount;

	TArray<uint32> BoneHashArray;
	MemoryReader->ReadArray(BaseModel.BoneIDsPtr, BoneHashArray, BaseModel.BoneCount);
	BoneIndexVariant BoneHashVariant;
	ReadBoneVariantInfo(MemoryReader, BaseModel.BoneIDsPtr, BoneCount, BaseModel.BoneIndexSize, BoneHashVariant);

	TArray<uint16> BoneParentArray;
	MemoryReader->ReadArray(BaseModel.BoneParentsPtr, BoneParentArray, ChildBoneSize);
	BoneIndexVariant BoneParentVariant;
	ReadBoneVariantInfo(MemoryReader, BaseModel.BoneParentsPtr, ChildBoneSize, BaseModel.BoneParentSize,
	                    BoneParentVariant);

	TArray<FCastBoneTransform> BoneGlobalTransformArray;
	MemoryReader->ReadArray(BaseModel.BaseTransformPtr, BoneGlobalTransformArray, BaseModel.BoneCount);
	TArray<FVector3f> BoneLocalTranslationArray;
	static_assert(sizeof(FVector3f) == 12, "FVector3f must be 12 bytes!");
	MemoryReader->ReadArray(BaseModel.TranslationsPtr, BoneLocalTranslationArray, ChildBoneSize);
	TArray<FVector4f> BoneLocalRotationArray;
	static_assert(sizeof(FVector4f) == 16, "FVector4f must be 16 bytes!");
	MemoryReader->ReadArray(BaseModel.RotationsPtr, BoneLocalRotationArray, ChildBoneSize);

	for (uint32 BoneIdx = 0; BoneIdx < BaseModel.BoneCount; ++BoneIdx)
	{
		FCastBoneInfo& Bone = SkeletonInfo.Bones.AddDefaulted_GetRef();

		// 骨骼名
		uint32 BoneNameHash;
		auto HashVisitor = [&](auto& InBoneHashArray)
		{
			BoneNameHash = InBoneHashArray[BoneIdx];
		};
		Visit(HashVisitor, BoneHashVariant);

		FCoDAssetDatabase::Get().AssetName_QueryValueRetName(BoneNameHash, Bone.BoneName, "bone");

		// 骨骼父节点
		if (BoneIdx >= BaseModel.RootBoneCount)
		{
			auto ParentVisitor = [&](auto& ParentArray)
			{
				Bone.ParentIndex = ParentArray[BoneIdx - BaseModel.RootBoneCount];
			};
			Visit(ParentVisitor, BoneParentVariant);

			if (BoneIdx < BaseModel.BoneCount)
			{
				Bone.ParentIndex = BoneIdx - Bone.ParentIndex;
			}
			else
			{
				bool bIsCosmetic = true;
			}
		}
		else
		{
			Bone.ParentIndex = -1;
		}

		Bone.WorldPosition = BoneGlobalTransformArray[BoneIdx].Translation;
		Bone.WorldRotation = BoneGlobalTransformArray[BoneIdx].Rotation;

		// 计算局部变换
		if (Bone.ParentIndex != -1)
		{
			FVector3f ParentBoneLocation = SkeletonInfo.Bones[Bone.ParentIndex].WorldPosition;
			FVector4f ParentBoneRotation = SkeletonInfo.Bones[Bone.ParentIndex].WorldRotation;
			const FQuat ParentGlobalRotationInverse = FQuat(
				ParentBoneRotation.X, ParentBoneRotation.Y, ParentBoneRotation.Z, ParentBoneRotation.W
			).Inverse();

			const FVector RelativePosition = FVector(
				Bone.WorldPosition - SkeletonInfo.Bones[Bone.ParentIndex].WorldPosition
			);
			const FVector RotatedVector = ParentGlobalRotationInverse.RotateVector(RelativePosition);
			Bone.LocalPosition = FVector3f(RotatedVector);

			const FQuat CurrentGlobalRotation = FQuat(
				Bone.WorldRotation.X, Bone.WorldRotation.Y, Bone.WorldRotation.Z, Bone.WorldRotation.W
			);

			const FQuat LocalRotationQuat = ParentGlobalRotationInverse * CurrentGlobalRotation;

			Bone.LocalRotation = FVector4f(
				LocalRotationQuat.X, LocalRotationQuat.Y, LocalRotationQuat.Z, LocalRotationQuat.W
			);
		}
		else
		{
			Bone.LocalPosition = Bone.WorldPosition;
			Bone.LocalRotation = Bone.WorldRotation;
		}
	}
}

void CoDBonesHelper::ReadBoneVariantInfo(TSharedPtr<IMemoryReader>& MemoryReader, const uint64& ReadAddr,
                                         uint64 BoneLen, int32 BoneIndexSize, BoneIndexVariant& OutBoneParentVariant)
{
	switch (BoneIndexSize)
	{
	case 1:
		{
			OutBoneParentVariant.Emplace<TArray<uint8>>();
			MemoryReader->ReadArray<uint8>(ReadAddr, OutBoneParentVariant.Get<TArray<uint8>>(), BoneLen);
			break;
		}
	case 2:
		{
			OutBoneParentVariant.Emplace<TArray<uint16>>();
			MemoryReader->ReadArray<uint16>(ReadAddr, OutBoneParentVariant.Get<TArray<uint16>>(), BoneLen);
			break;
		}
	case 4:
		{
			OutBoneParentVariant.Emplace<TArray<uint32>>();
			MemoryReader->ReadArray<uint32>(ReadAddr, OutBoneParentVariant.Get<TArray<uint32>>(), BoneLen);
			break;
		}
	default:
		UE_LOG(LogTemp, Error, TEXT("Unsupported BoneIndexSize: %d"), BoneIndexSize);
		break;
	}
}
