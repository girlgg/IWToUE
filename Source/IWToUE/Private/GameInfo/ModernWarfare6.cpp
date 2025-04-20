#include "GameInfo/ModernWarfare6.h"

#include "CastManager/CastAnimation.h"
#include "CDN/CoDCDNDownloader.h"
#include "Structures/MW6SPGameStructures.h"
#include "ThirdSupport/SABSupport.h"
#include "Utils/CoDAssetHelper.h"
#include "Utils/CoDBonesHelper.h"
#include "WraithX/CoDAssetType.h"
#include "WraithX/GameProcess.h"

FModernWarfare6::FModernWarfare6(FCordycepProcess* InProcess)
	: TBaseGame(InProcess)
{
	GFXMAP_POOL_IDX = 50;
}

TArray<FCastTextureInfo> FModernWarfare6::PopulateMaterial(const FMW6Material& Material)
{
	TArray<FMW6GfxImage> Images;
	Images.Reserve(Material.ImageCount);

	for (uint32 i = 0; i < Material.ImageCount; i++)
	{
		const uint64 ImagePtr = Process->ReadMemory<uint64>(Material.ImageTable + i * 8);
		FMW6GfxImage Image = Process->ReadMemory<FMW6GfxImage>(ImagePtr);
		Images.Add(Image);
	}

	TArray<FCastTextureInfo> Textures;

	for (int i = 0; i < Material.TextureCount; i++)
	{
		FMW6MaterialTextureDef TextureDef = Process->ReadMemory<FMW6MaterialTextureDef>(
			Material.TextureTable + i * sizeof(FMW6MaterialTextureDef));
		FMW6GfxImage Image = Images[TextureDef.ImageIdx];

		int UVMapIndex = 0;

		uint64 Hash = Image.Hash & 0x0FFFFFFFFFFFFFFF;

		FString ImageName = FString::Printf(TEXT("ximage_%llx"), Hash);

		FString TextureSemantic = FString::Printf(TEXT("unk_semantic_0x%x"), TextureDef.Index);

		FCastTextureInfo TextureInfo;
		TextureInfo.TextureName = ImageName;
		TextureInfo.TextureSemantic = TextureSemantic;
		Textures.Add(TextureInfo);
	}

	return Textures;
}

float FModernWarfare6::GetSurfaceScale(const FMW6XSurface& Surface)
{
	return Surface.OverrideScale != -1 ? Surface.OverrideScale : FMath::Max3(Surface.Min, Surface.Scale, Surface.Max);
}

FVector3f FModernWarfare6::GetSurfaceOffset(const FMW6XSurface& Surface)
{
	return Surface.OverrideScale != -1
		       ? FVector3f::Zero()
		       : FVector3f(Surface.OffsetsX, Surface.OffsetsY, Surface.OffsetsZ);
}

uint64 FModernWarfare6::GetXPakKey(const FMW6XModelSurfs& XModelSurfs, const FMW6XSurfaceShared& Shared)
{
	return XModelSurfs.XPakKey;
}

uint64 FModernWarfare6::GetXyzOffset(const FMW6XSurface& Surface)
{
	return Surface.XyzOffset;
}

uint64 FModernWarfare6::GetTangentFrameOffset(const FMW6XSurface& Surface)
{
	return Surface.TangentFrameOffset;
}

uint64 FModernWarfare6::GetTexCoordOffset(const FMW6XSurface& Surface)
{
	return Surface.TexCoordOffset;
}

uint64 FModernWarfare6::GetColorOffset(const FMW6XSurface& Surface)
{
	return Surface.ColorOffset;
}

uint64 FModernWarfare6::GetIndexDataOffset(const FMW6XSurface& Surface)
{
	return Surface.IndexDataOffset;
}

uint64 FModernWarfare6::GetPackedIndicesOffset(const FMW6XSurface& Surface)
{
	return Surface.PackedIndicesOffset;
}

bool FModernWarfare6::ReadXAnim(FWraithXAnim& OutAnim, TSharedPtr<FGameProcess> ProcessInstance,
                                TSharedPtr<FCoDAnim> AnimAddr)
{
	return false;
	/*auto AnimData = ProcessInstance->ReadMemory<FMW6XAnim>(AnimAddr->AssetPointer);

	if (AnimData.DataInfo.StreamInfoPtr == 0)
	{
		return false;
	}
	OutAnim.AnimationName = AnimAddr->AssetName;
	OutAnim.FrameCount = AnimAddr->FrameCount;
	OutAnim.FrameRate = AnimData.Framerate;
	OutAnim.LoopingAnimation = (AnimData.Padding2[4] & 1);
	OutAnim.AdditiveAnimation = AnimData.AssetType == 0x6;
	OutAnim.AnimType = AnimData.AssetType;
	FMW6XAnimDeltaParts AnimDeltaData = ProcessInstance->ReadMemory<FMW6XAnimDeltaParts>(AnimData.DeltaPartsPtr);
	OutAnim.BoneIndexSize = 4;

	OutAnim.NoneRotatedBoneCount = AnimData.NoneRotatedBoneCount;
	OutAnim.TwoDRotatedBoneCount = AnimData.TwoDRotatedBoneCount;
	OutAnim.NormalRotatedBoneCount = AnimData.NormalRotatedBoneCount;
	OutAnim.TwoDStaticRotatedBoneCount = AnimData.TwoDStaticRotatedBoneCount;
	OutAnim.NormalStaticRotatedBoneCount = AnimData.NormalStaticRotatedBoneCount;
	OutAnim.NormalTranslatedBoneCount = AnimData.NormalTranslatedBoneCount;
	OutAnim.PreciseTranslatedBoneCount = AnimData.PreciseTranslatedBoneCount;
	OutAnim.StaticTranslatedBoneCount = AnimData.StaticTranslatedBoneCount;
	OutAnim.TotalBoneCount = AnimData.TotalBoneCount;
	OutAnim.NotificationCount = AnimData.NotetrackCount;

	OutAnim.ReaderInformationPointer = AnimAddr->AssetPointer;
	for (uint16 BoneIdx = 0; BoneIdx < AnimData.TotalBoneCount; BoneIdx++)
	{
		const uint32 BoneID = ProcessInstance->ReadMemory<uint32>(AnimData.BoneIDsPtr + BoneIdx * 4);
		FString BoneName;
		FCoDAssetDatabase::Get().AssetName_QueryValueRetName(BoneID, BoneName, "bone");
		OutAnim.Reader.BoneNames.Add(BoneName);
	}
	for (uint8 TrackIdx = 0; TrackIdx < AnimData.NotetrackCount; TrackIdx++)
	{
		FMW6XAnimNoteTrack NoteTrack = ProcessInstance->ReadMemory<FMW6XAnimNoteTrack>(
			AnimData.NotificationsPtr + TrackIdx * sizeof(FMW6XAnimNoteTrack));
		FString NoteTrackName = ProcessInstance->LoadStringEntry(NoteTrack.Name);
		OutAnim.Reader.Notetracks.Emplace(NoteTrackName, NoteTrack.Time * (int32)((float)AnimData.FrameCount)); // ???
	}
	OutAnim.DeltaTranslationPtr = AnimDeltaData.DeltaTranslationsPtr;
	OutAnim.Delta2DRotationsPtr = AnimDeltaData.Delta2DRotationsPtr;
	OutAnim.Delta3DRotationsPtr = AnimDeltaData.Delta3DRotationsPtr;

	OutAnim.RotationType = EAnimationKeyTypes::DivideBySize;
	OutAnim.TranslationType = EAnimationKeyTypes::MinSizeTable;
	OutAnim.SupportsInlineIndicies = true;

	return true;*/
}

uint8 FModernWarfare6::ReadXImage(TSharedPtr<FGameProcess> ProcessInstance,
                                  TSharedPtr<FCoDImage> ImageAddr, TArray<uint8>& ImageDataArray)
{
	return false;
	/*FMW6GfxImage ImageInfo = ProcessInstance->ReadMemory<FMW6GfxImage>(ImageAddr->AssetPointer);

	if (ImageInfo.LoadedImagePtr)
	{
		uint32 ImageSize = ImageInfo.BufferSize;
		ImageDataArray.SetNumUninitialized(ImageSize);

		uint64 ReadSize =
			ProcessInstance->ReadArray(ImageInfo.LoadedImagePtr, ImageDataArray, ImageInfo.BufferSize);
		if (ReadSize != ImageSize)
		{
			ImageDataArray.Empty();
		}
	}
	else
	{
		if (ImageInfo.ImageFormat == 0)
		{
			return 0;
		}
		uint32 MipCount = FMath::Min(static_cast<uint32>(ImageInfo.MipCount), 32u);
		FMW6GfxMipArray Mips;
		uint64 ReadSize = ProcessInstance->ReadArray(ImageInfo.MipMaps, Mips.MipMaps, MipCount);
		if (ReadSize != MipCount * sizeof(FMW6GfxMip))
		{
			return 0;
		}
		int32 Fallback = 0;
		int32 HighestIndex = ImageInfo.MipCount - 1;
		for (uint32 MipIdx = 0; MipIdx < MipCount; ++MipIdx)
		{
			if (ProcessInstance->GetDecrypt()->ExistsKey(Mips.MipMaps[MipIdx].HashID))
			{
				Fallback = MipIdx;
			}
		}

		ImageInfo.ImageFormat = GMW6DXGIFormats[ImageInfo.ImageFormat];

		if (Fallback != MipCount - 1 && ProcessInstance->GetCDNDownloader())
		{
			ProcessInstance->GetCDNDownloader()->ExtractCDNObject(ImageDataArray, Mips.MipMaps[HighestIndex].HashID,
			                                                      Mips.GetImageSize(HighestIndex));
		}
		// TODO 普通硬盘存储图像
		if (ImageDataArray.IsEmpty())
		{
			// uint32_t PackageSize = 0;
			// ImageData = CoDAssets::GamePackageCache->ExtractPackageObject(Mips.MipMaps[Fallback].HashID, Mips.GetImageSize(Fallback), PackageSize);
			// ImageSize = PackageSize;
			// HighestIndex = Fallback;
		}
	}
	return ImageInfo.ImageFormat;*/
}

bool FModernWarfare6::ReadXModel(FWraithXModel& OutModel, TSharedPtr<FGameProcess> ProcessInstance,
                                 TSharedPtr<FCoDModel> ModelAddr)
{
	return true;
}

bool FModernWarfare6::ReadXMaterial(FWraithXMaterial& OutMaterial, TSharedPtr<FGameProcess> ProcessInstance,
                                    uint64 MaterialHandle)
{
	return false;
	/*if (ProcessInstance->GetCurrentGameFlag() == CoDAssets::ESupportedGameFlags::SP)
	{
		FMW6SPMaterial MaterialData = ProcessInstance->ReadMemory<FMW6SPMaterial>(MaterialHandle);
	}
	else
	{
		FMW6Material MaterialData = ProcessInstance->ReadMemory<FMW6Material>(MaterialHandle);

		FCoDAssetDatabase::Get().
			AssetName_QueryValueRetName(MaterialData.Hash, OutMaterial.MaterialName,TEXT("ximage"));

		TArray<FMW6GfxImage> Images;
		Images.Reserve(MaterialData.ImageCount);
		for (uint32 i = 0; i < MaterialData.ImageCount; i++)
		{
			const uint64 ImagePtr = ProcessInstance->ReadMemory<uint64>(MaterialData.ImageTable + i * sizeof(uint64));
			FMW6GfxImage Image = ProcessInstance->ReadMemory<FMW6GfxImage>(ImagePtr);
			Images.Add(Image);
		}

		OutMaterial.Images.Empty();

		for (int i = 0; i < MaterialData.TextureCount; i++)
		{
			FMW6MaterialTextureDef TextureDef = ProcessInstance->ReadMemory<FMW6MaterialTextureDef>(
				MaterialData.TextureTable + i * sizeof(FMW6MaterialTextureDef));
			FMW6GfxImage Image = Images[TextureDef.ImageIdx];

			FString ImageName;

			FCoDAssetDatabase::Get().AssetName_QueryValueRetName(Image.Hash, ImageName,TEXT("ximage"));

			// 一些占位符之类的，没有用
			if (!ImageName.IsEmpty() && ImageName[0] != '$')
			{
				FWraithXImage& ImageOut = OutMaterial.Images.AddDefaulted_GetRef();
				ImageOut.ImageName = ImageName;
				ImageOut.SemanticHash = TextureDef.Index;
			}
		}
	}

	return true;*/
}

bool FModernWarfare6::ReadXSound(FWraithXSound& OutSound, TSharedPtr<FGameProcess> ProcessInstance,
                                 TSharedPtr<FCoDAsset> InSound)
{
	return false;
	/*FMW6SoundAsset SoundData = ProcessInstance->ReadMemory<FMW6SoundAsset>(InSound->AssetPointer);
	if (SoundData.StreamKey)
	{
		TArray<uint8> SoundBuffer = ProcessInstance->GetDecrypt()->ExtractXSubPackage(
			SoundData.StreamKey, ((SoundData.Size + SoundData.SeekTableSize + 4095) & 0xFFFFFFFFFFFFFFF0));
		if (SoundBuffer.IsEmpty()) return false;
		SoundBuffer.RemoveAt(0, SoundData.SeekTableSize);
		return SABSupport::DecodeOpusInterLeaved(OutSound, SoundBuffer,
		                                         0, SoundData.FrameRate,
		                                         SoundData.ChannelCount, SoundData.FrameCount);
	}
	TArray<uint8> SoundBuffer = ProcessInstance->GetDecrypt()->ExtractXSubPackage(
		SoundData.StreamKeyEx, ((SoundData.LoadedSize + SoundData.SeekTableSize + 4095) & 0xFFFFFFFFFFFFFFF0));
	if (SoundBuffer.IsEmpty()) return false;
	SoundBuffer.RemoveAt(0, 32 + SoundData.SeekTableSize);
	return SABSupport::DecodeOpusInterLeaved(OutSound, SoundBuffer,
	                                         0, SoundData.FrameRate,
	                                         SoundData.ChannelCount, SoundData.FrameCount);*/
}

void FModernWarfare6::LoadXAnim(TSharedPtr<FGameProcess> ProcessInstance, FWraithXAnim& InAnim,
                                FCastAnimationInfo& OutAnim)
{
	/*FMW6XAnim AnimHeader = ProcessInstance->ReadMemory<FMW6XAnim>(InAnim.ReaderInformationPointer);

	uint8* DataByte = nullptr;
	int16* DataShort = nullptr;
	int32* DataInt = nullptr;

	TArray<uint8*> RandomDataBytes;
	TArray<int16*> RandomDataShorts;

	OutAnim.Type = ECastAnimationType::Relative;

	TArray<uint32> AnimPackedInfo;
	ProcessInstance->ReadArray(AnimHeader.DataInfo.PackedInfoPtr, AnimPackedInfo,
	                           AnimHeader.DataInfo.PackedInfoCount);
	TArray<uint8> IndicesBuffer;
	ProcessInstance->ReadArray(AnimHeader.IndicesPtr, IndicesBuffer,
	                           AnimHeader.FrameCount >= 0x100 ? AnimHeader.IndexCount * 2 : AnimHeader.IndexCount);

	uint16* Indices = reinterpret_cast<uint16*>(IndicesBuffer.GetData());

	TArray<TArray<uint8>> AnimBuffers;
	for (uint32 CountIdx = 0; CountIdx < AnimHeader.DataInfo.OffsetCount; ++CountIdx)
	{
		FMW6XAnimStreamInfo StreamInfo = ProcessInstance->ReadMemory<FMW6XAnimStreamInfo>(
			AnimHeader.DataInfo.StreamInfoPtr + CountIdx * sizeof(FMW6XAnimStreamInfo));

		TArray<uint8> AnimBuffer = ProcessInstance->GetDecrypt()->ExtractXSubPackage(
			StreamInfo.StreamKey, StreamInfo.Size);
		AnimBuffers.Add(AnimBuffer);
		if (CountIdx == 0)
		{
			if (AnimHeader.DataInfo.DataByteOffset != -1)
				DataByte = AnimBuffers[0].GetData() + AnimHeader.DataInfo.DataByteOffset;

			if (AnimHeader.DataInfo.DataShortOffset != -1)
				DataShort = reinterpret_cast<int16*>(AnimBuffers[0].GetData() + AnimHeader.DataInfo.
					DataShortOffset);

			if (AnimHeader.DataInfo.DataIntOffset != -1)
				DataInt = reinterpret_cast<int32*>(AnimBuffers[0].GetData() + AnimHeader.DataInfo.
					DataIntOffset);
		}
		uint32 RandomDataAOffset = ProcessInstance->ReadMemory<uint32>(AnimHeader.DataInfo.OffsetPtr2 + CountIdx * 4);
		uint32 RandomDataBOffset = ProcessInstance->ReadMemory<uint32>(AnimHeader.DataInfo.OffsetPtr + CountIdx * 4);

		if (RandomDataAOffset != -1)
			RandomDataBytes.Add(AnimBuffers[CountIdx].GetData() + RandomDataAOffset);
		else
			RandomDataBytes.Add(nullptr);
		if (RandomDataBOffset != -1)
			RandomDataShorts.Add(reinterpret_cast<int16*>(AnimBuffers[CountIdx].GetData() + RandomDataBOffset));
		else
			RandomDataShorts.Add(nullptr);
	}
	FMW6XAnimBufferState State;
	State.OffsetCount = AnimHeader.DataInfo.OffsetCount;
	State.PackedPerFrameInfo = AnimPackedInfo;

	int32 CurrentBoneIndex = 0;
	int32 CurrentSize = InAnim.NoneRotatedBoneCount;
	bool ByteFrames = InAnim.FrameCount < 0x100;

	while (CurrentBoneIndex < CurrentSize)
	{
		OutAnim.AddRotationKey(InAnim.Reader.BoneNames[CurrentBoneIndex++], 0, FVector4(0, 0, 0, 1));
	}
	CurrentSize += InAnim.TwoDRotatedBoneCount;
	// 2D Bones
	while (CurrentBoneIndex < CurrentSize)
	{
		int16 TableSize = *DataShort++;

		if ((TableSize >= 0x40) && !ByteFrames)
			DataShort += ((TableSize - 1) >> 8) + 2;

		for (int16 Idx = 0; Idx <= TableSize; ++Idx)
		{
			uint32 Frame = 0;
			if (ByteFrames)
			{
				Frame = *DataByte++;
			}
			else
			{
				Frame = TableSize >= 0x40 ? *Indices++ : *DataShort++;
			}
			MW6XAnimCalculateBufferIndex(&State, TableSize + 1, Idx);
			int16* RandomDataShort = RandomDataShorts[State.BufferIndex] + 2 * State.BufferOffset;
			float RZ = (float)RandomDataShort[0] * 0.000030518509f;
			float RW = (float)RandomDataShort[1] * 0.000030518509f;

			OutAnim.AddRotationKey(InAnim.Reader.BoneNames[CurrentBoneIndex], Frame, FVector4(0, 0, RZ, RW));
		}
		MW6XAnimIncrementBuffers(&State, TableSize + 1, 2, RandomDataShorts);
		CurrentBoneIndex++;
	}

	CurrentSize += InAnim.NormalRotatedBoneCount;
	// 3D Rotations
	while (CurrentBoneIndex < CurrentSize)
	{
		int16 TableSize = *DataShort++;
		if ((TableSize >= 0x40) && !ByteFrames)
			DataShort += ((TableSize - 1) >> 8) + 2;
		for (int32 i = 0; i < TableSize + 1; ++i)
		{
			uint32 Frame = 0;
			if (ByteFrames)
			{
				Frame = *DataByte++;
			}
			else
			{
				Frame = TableSize >= 0x40 ? *Indices++ : *DataShort++;
			}
			MW6XAnimCalculateBufferIndex(&State, TableSize + 1, i);

			int16* RandomDataShort = RandomDataShorts[State.BufferIndex] + 4 * State.BufferOffset;
			float RX = (float)RandomDataShort[0] * 0.000030518509f;
			float RY = (float)RandomDataShort[1] * 0.000030518509f;
			float RZ = (float)RandomDataShort[2] * 0.000030518509f;
			float RW = (float)RandomDataShort[3] * 0.000030518509f;

			OutAnim.AddRotationKey(InAnim.Reader.BoneNames[CurrentBoneIndex], Frame, FVector4(RX, RY, RZ, RW));
		}
		MW6XAnimIncrementBuffers(&State, TableSize + 1, 4, RandomDataShorts);
		CurrentBoneIndex++;
	}

	CurrentSize += InAnim.TwoDStaticRotatedBoneCount;
	// 2D Static Rotations
	while (CurrentBoneIndex < CurrentSize)
	{
		float RZ = (float)*DataShort++ * 0.000030518509f;
		float RW = (float)*DataShort++ * 0.000030518509f;

		OutAnim.AddRotationKey(InAnim.Reader.BoneNames[CurrentBoneIndex++], 0, FVector4(0, 0, RZ, RW));
	}
	CurrentSize += InAnim.NormalStaticRotatedBoneCount;
	// 3D Static Rotations
	while (CurrentBoneIndex < CurrentSize)
	{
		float RX = (float)*DataShort++ * 0.000030518509f;
		float RY = (float)*DataShort++ * 0.000030518509f;
		float RZ = (float)*DataShort++ * 0.000030518509f;
		float RW = (float)*DataShort++ * 0.000030518509f;

		OutAnim.AddRotationKey(InAnim.Reader.BoneNames[CurrentBoneIndex++], 0, FVector4(RX, RY, RZ, RW));
	}

	CurrentBoneIndex = 0;
	CurrentSize = InAnim.NormalTranslatedBoneCount;
	while (CurrentBoneIndex++ < CurrentSize)
	{
		int16 BoneIndex = *DataShort++;
		int16 TableSize = *DataShort++;
		if ((TableSize >= 0x40) && !ByteFrames)
			DataShort += ((TableSize - 1) >> 8) + 2;
		float MinsVecX = *(float*)DataInt++;
		float MinsVecY = *(float*)DataInt++;
		float MinsVecZ = *(float*)DataInt++;
		float FrameVecX = *(float*)DataInt++;
		float FrameVecY = *(float*)DataInt++;
		float FrameVecZ = *(float*)DataInt++;
		for (int32 i = 0; i <= TableSize; ++i)
		{
			int32 Frame = 0;
			if (ByteFrames)
			{
				Frame = *DataByte++;
			}
			else
			{
				Frame = TableSize >= 0x40 ? *Indices++ : *DataShort++;
			}
			MW6XAnimCalculateBufferIndex(&State, TableSize + 1, i);

			uint8* RandomDataByte = RandomDataBytes[State.BufferIndex] + 3 * State.BufferOffset;
			float TranslationX = (FrameVecX * (float)RandomDataByte[0]) + MinsVecX;
			float TranslationY = (FrameVecY * (float)RandomDataByte[1]) + MinsVecY;
			float TranslationZ = (FrameVecZ * (float)RandomDataByte[2]) + MinsVecZ;

			OutAnim.AddTranslationKey(InAnim.Reader.BoneNames[BoneIndex], Frame,
			                          FVector(TranslationX, TranslationY, TranslationZ));
		}
		MW6XAnimIncrementBuffers(&State, TableSize + 1, 3, RandomDataBytes);
	}
	CurrentBoneIndex = 0;
	CurrentSize = InAnim.PreciseTranslatedBoneCount;
	while (CurrentBoneIndex++ < CurrentSize)
	{
		int16 BoneIndex = *DataShort++;
		int16 TableSize = *DataShort++;

		if ((TableSize >= 0x40) && !ByteFrames)
			DataShort += ((TableSize - 1) >> 8) + 2;
		float MinsVecX = *(float*)DataInt++;
		float MinsVecY = *(float*)DataInt++;
		float MinsVecZ = *(float*)DataInt++;
		float FrameVecX = *(float*)DataInt++;
		float FrameVecY = *(float*)DataInt++;
		float FrameVecZ = *(float*)DataInt++;

		for (int i = 0; i <= TableSize; ++i)
		{
			int Frame = 0;
			if (ByteFrames)
			{
				Frame = *DataByte++;
			}
			else
			{
				Frame = TableSize >= 0x40 ? *Indices++ : *DataShort++;
			}
			MW6XAnimCalculateBufferIndex(&State, TableSize + 1, i);
			int16* RandomDataShort = RandomDataShorts[State.BufferIndex] + 3 * State.BufferOffset;

			float TranslationX = (FrameVecX * (float)(uint16)RandomDataShort[0]) + MinsVecX;
			float TranslationY = (FrameVecY * (float)(uint16)RandomDataShort[1]) + MinsVecY;
			float TranslationZ = (FrameVecZ * (float)(uint16)RandomDataShort[2]) + MinsVecZ;

			OutAnim.AddTranslationKey(InAnim.Reader.BoneNames[BoneIndex], Frame,
			                          FVector(TranslationX, TranslationY, TranslationZ));
		}
		MW6XAnimIncrementBuffers(&State, TableSize + 1, 3, RandomDataShorts);
	}
	CurrentBoneIndex = 0;
	CurrentSize = InAnim.StaticTranslatedBoneCount;
	while (CurrentBoneIndex++ < CurrentSize)
	{
		FVector3f Vec = *(FVector3f*)DataInt;
		DataInt += 3;
		int16 BoneIndex = *DataShort++;

		OutAnim.AddTranslationKey(InAnim.Reader.BoneNames[BoneIndex], 0, FVector(Vec));
	}*/
}

void FModernWarfare6::LoadXModel(TSharedPtr<FGameProcess> ProcessInstance, FWraithXModel& BaseModel,
                                 FWraithXModelLod& ModelLod, FCastModelInfo& ResultModel)
{
	/*FMW6XModelSurfs MeshInfo = ProcessInstance->ReadMemory<FMW6XModelSurfs>(ModelLod.LODStreamInfoPtr);
	FMW6XSurfaceShared BufferInfo = ProcessInstance->ReadMemory<FMW6XSurfaceShared>(MeshInfo.Shared);

	TArray<uint8> MeshDataBuffer;
	if (BufferInfo.Data)
	{
		ProcessInstance->ReadArray(BufferInfo.Data, MeshDataBuffer, BufferInfo.DataSize);
	}
	else
	{
		MeshDataBuffer = ProcessInstance->GetDecrypt()->ExtractXSubPackage(MeshInfo.XPakKey, BufferInfo.DataSize);
	}

	CoDBonesHelper::ReadXModelBones(ProcessInstance, BaseModel, ModelLod, ResultModel);

	if (MeshDataBuffer.IsEmpty())
	{
		return;
	}

	for (FWraithXModelSubmesh& Submesh : ModelLod.Submeshes)
	{
		FCastMeshInfo& Mesh = ResultModel.Meshes.AddDefaulted_GetRef();
		Mesh.MaterialIndex = Submesh.MaterialIndex;

		FBufferReader VertexPosReader(MeshDataBuffer.GetData() + Submesh.VertexOffset,
		                              Submesh.VertexOffset * sizeof(uint64), false);
		FBufferReader VertexTangentReader(MeshDataBuffer.GetData() + Submesh.VertexTangentOffset,
		                                  Submesh.VertexOffset * sizeof(uint32), false);
		FBufferReader VertexUVReader(MeshDataBuffer.GetData() + Submesh.VertexUVsOffset,
		                             Submesh.VertexOffset * sizeof(uint32), false);
		FBufferReader FaceIndiciesReader(MeshDataBuffer.GetData() + Submesh.FacesOffset,
		                                 Submesh.FacesOffset * sizeof(uint16) * 3, false);

		// 顶点权重
		Mesh.VertexWeights.SetNum(Submesh.VertexCount);
		int32 WeightDataLength = 0;
		for (int32 WeightIdx = 0; WeightIdx < 8; ++WeightIdx)
		{
			WeightDataLength += (WeightIdx + 1) * 4 * Submesh.WeightCounts[WeightIdx];
		}
		// TODO 多骨骼支持
		if (ResultModel.Skeletons.Num() > 0)
		{
			FBufferReader VertexWeightReader(MeshDataBuffer.GetData() + Submesh.WeightsOffset, WeightDataLength, false);
			uint32 WeightDataIndex = 0;
			for (int32 i = 0; i < 8; ++i)
			{
				for (int32 WeightIdx = 0; WeightIdx < i + 1; ++WeightIdx)
				{
					for (uint32 LocalWeightDataIndex = WeightDataIndex;
					     LocalWeightDataIndex < WeightDataIndex + Submesh.WeightCounts[i]; ++LocalWeightDataIndex)
					{
						Mesh.VertexWeights[LocalWeightDataIndex].WeightCount = WeightIdx + 1;
						uint16 BoneValue;
						VertexWeightReader << BoneValue;
						Mesh.VertexWeights[LocalWeightDataIndex].BoneValues[WeightIdx] = BoneValue;

						if (WeightIdx > 0)
						{
							uint16 WeightValue;
							VertexWeightReader << WeightValue;
							Mesh.VertexWeights[LocalWeightDataIndex].WeightValues[WeightIdx] = WeightValue / 65536.f;
							Mesh.VertexWeights[LocalWeightDataIndex].WeightValues[0] -= Mesh.VertexWeights[
									LocalWeightDataIndex].
								WeightValues[WeightIdx];
						}
						else
						{
							VertexWeightReader.Seek(VertexWeightReader.Tell() + 2);
						}
					}
				}
				WeightDataIndex += Submesh.WeightCounts[i];
			}
		}

		Mesh.VertexPositions.Reserve(Submesh.VertexCount);
		Mesh.VertexTangents.Reserve(Submesh.VertexCount);
		Mesh.VertexNormals.Reserve(Submesh.VertexCount);
		Mesh.VertexUV.Reserve(Submesh.VertexCount);
		Mesh.VertexColor.Reserve(Submesh.VertexCount);
		// UV
		for (uint32 VertexIdx = 0; VertexIdx < Submesh.VertexCount; ++VertexIdx)
		{
			FVector3f& VertexPos = Mesh.VertexPositions.AddDefaulted_GetRef();
			uint64 PackedPos;
			VertexPosReader << PackedPos;
			VertexPos = FVector3f{
				((PackedPos >> 00 & 0x1FFFFF) * (1.0f / 0x1FFFFF * 2.0f) - 1.0f) * Submesh.Scale + Submesh.XOffset,
				((PackedPos >> 21 & 0x1FFFFF) * (1.0f / 0x1FFFFF * 2.0f) - 1.0f) * Submesh.Scale + Submesh.YOffset,
				((PackedPos >> 42 & 0x1FFFFF) * (1.0f / 0x1FFFFF * 2.0f) - 1.0f) * Submesh.Scale + Submesh.ZOffset
			};

			uint32 PackedTangentFrame;
			VertexTangentReader << PackedTangentFrame;
			FVector3f Normal, Tangent;
			UnpackCoDQTangent(PackedTangentFrame, Tangent, Normal);
			Mesh.VertexNormals.Add(Normal);
			Mesh.VertexTangents.Add(Tangent);

			uint16 HalfUvU, HalfUvV;
			VertexUVReader << HalfUvU << HalfUvV;
			FFloat16 HalfFloatU, HalfFloatV;
			HalfFloatU.Encoded = HalfUvU;
			HalfFloatV.Encoded = HalfUvV;
			Mesh.VertexUV.Emplace(HalfUvU, HalfFloatU);
		}
		// TODO Second UV
		// 顶点色
		if (Submesh.VertexColorOffset != 0xFFFFFFFF)
		{
			FBufferReader VertexColorReader(MeshDataBuffer.GetData() + Submesh.VertexColorOffset,
			                                Submesh.FacesOffset * sizeof(uint32), false);
			for (uint32 VertexIdx = 0; VertexIdx < Submesh.VertexCount; VertexIdx++)
			{
				uint32 VertexColor;
				VertexColorReader << VertexColor;
				Mesh.VertexColor.Add(VertexColor);
			}
		}
		// 面
		uint64 TableOffsetPtr = reinterpret_cast<uint64>(MeshDataBuffer.GetData()) + Submesh.PackedIndexTableOffset;
		uint64 PackedIndices = reinterpret_cast<uint64>(MeshDataBuffer.GetData()) + Submesh.PackedIndexBufferOffset;
		uint64 IndicesPtr = reinterpret_cast<uint64>(MeshDataBuffer.GetData()) + Submesh.FacesOffset;

		for (uint32 TriIdx = 0; TriIdx < Submesh.FaceCount; TriIdx++)
		{
			TArray<uint16> Faces;
			FCoDMeshHelper::UnpackFaceIndices(ProcessInstance, Faces, TableOffsetPtr, Submesh.PackedIndexTableCount,
			                                  PackedIndices, IndicesPtr, TriIdx, true);

			Mesh.Faces.Add(Faces[2]);
			Mesh.Faces.Add(Faces[1]);
			Mesh.Faces.Add(Faces[0]);
		}
	}*/
}

int32 FModernWarfare6::MW6XAnimCalculateBufferOffset(const FMW6XAnimBufferState* AnimState, const int32 Index,
                                                     const int32 Count)
{
	if (Count == 0)
		return 0;

	int32 Mask = Index + Count - 1;
	int32 Start = Index >> 5;
	int32 End = Mask >> 5;
	int32 Result = 0;

	for (int32 i = Start; i <= End; i++)
	{
		if (AnimState->PackedPerFrameInfo[i] == 0)
			continue;

		uint32 maskA = 0xFFFFFFFF;
		uint32 maskB = 0xFFFFFFFF;

		// If we're at the start or end, we need to calculate trailing bit masks.
		if (i == Start)
			maskA = 0xFFFFFFFF >> (Index & 0x1F);
		if (i == End)
			maskB = 0xFFFFFFFF << (31 - (Mask & 0x1F));

		Result += FPlatformMath::CountBits(AnimState->PackedPerFrameInfo[i] & maskA & maskB);
	}

	return Result;
}

void FModernWarfare6::MW6XAnimCalculateBufferIndex(FMW6XAnimBufferState* AnimState, const int32 TableSize,
                                                   const int32 KeyFrameIndex)
{
	if (TableSize < 4 || AnimState->OffsetCount == 1)
	{
		AnimState->BufferIndex = 0;
		AnimState->BufferOffset = KeyFrameIndex;
	}
	else
	{
		for (int32 i = 0; i < AnimState->OffsetCount; ++i)
		{
			if (((0x80000000 >> ((KeyFrameIndex + (i * TableSize) + AnimState->PackedPerFrameOffset) & 0x1F)) &
					AnimState->PackedPerFrameInfo[(KeyFrameIndex + (i * TableSize) + AnimState->PackedPerFrameOffset) >>
						5])
				!= 0)
			{
				AnimState->BufferIndex = i;
				AnimState->BufferOffset = MW6XAnimCalculateBufferOffset(
					AnimState, AnimState->PackedPerFrameOffset + TableSize * AnimState->BufferIndex, KeyFrameIndex);
				return;
			}
		}
	}
}
