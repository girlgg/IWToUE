#include "GameInfo/ModernWarfare5AssetHandler.h"

#include "CDN/CoDCDNDownloader.h"
#include "Database/CoDDatabaseService.h"
#include "Interface/IMemoryReader.h"
#include "MapImporter/XSub.h"
#include "Structures/MW5GameStructures.h"
#include "ThirdSupport/SABSupport.h"
#include "Utils/CoDAssetHelper.h"
#include "Utils/CoDBonesHelper.h"
#include "WraithX/CoDAssetType.h"
#include "WraithX/LocateGameInfo.h"

bool ModernWarfare5AssetHandler::ReadModelData(TSharedPtr<FCoDModel> ModelInfo, FWraithXModel& OutModel)
{
	FMW5XModel ModelData;
	MemoryReader->ReadMemory<FMW5XModel>(ModelInfo->AssetPointer, ModelData);

	if (ModelInfo->AssetName.IsEmpty())
	{
		if (ModelData.NamePtr != 0)
		{
			MemoryReader->ReadString(ModelData.NamePtr, OutModel.ModelName);
			OutModel.ModelName = FCoDAssetNameHelper::NoIllegalSigns(FPaths::GetBaseFilename(OutModel.ModelName));
		}
		else
		{
			OutModel.ModelName = FCoDDatabaseService::Get().GetPrintfAssetName(ModelData.Hash, TEXT("XModel"));
		}
	}
	else
	{
		OutModel.ModelName = ModelInfo->AssetName;
	}

	if (ModelData.NumBones + ModelData.UnkBoneCount > 1 && ModelData.ParentListPtr == 0)
	{
		OutModel.BoneCount = 0;
		OutModel.RootBoneCount = 0;
	}
	else
	{
		OutModel.BoneCount = ModelData.NumBones + ModelData.UnkBoneCount;
		OutModel.RootBoneCount = ModelData.NumRootBones;
	}

	OutModel.BoneRotationData = EBoneDataTypes::DivideBySize;
	OutModel.IsModelStreamed = true;
	OutModel.BoneIDsPtr = ModelData.BoneIDsPtr;
	OutModel.BoneIndexSize = sizeof(uint32);
	OutModel.BoneParentsPtr = ModelData.ParentListPtr;
	OutModel.BoneParentSize = sizeof(uint16);
	OutModel.RotationsPtr = ModelData.RotationsPtr;
	OutModel.TranslationsPtr = ModelData.TranslationsPtr;
	OutModel.BaseTransformPtr = ModelData.BaseMatriciesPtr;

	for (uint32 LodIdx = 0; LodIdx < ModelData.NumLods; ++LodIdx)
	{
		FMW5XModelLod ModelLod;
		MemoryReader->ReadMemory<FMW5XModelLod>(ModelData.ModelLods + LodIdx * sizeof(FMW5XModelLod), ModelLod);

		FWraithXModelLod& LodRef = OutModel.ModelLods.AddDefaulted_GetRef();

		LodRef.LodDistance = ModelLod.LodDistance[FMath::Clamp(LodIdx, 0, 2)];
		LodRef.LODStreamInfoPtr = ModelLod.MeshPtr;
		uint64 XSurfacePtr = ModelLod.SurfsPtr;

		LodRef.Submeshes.Reserve(ModelLod.NumSurfs);
		for (uint32 SurfaceIdx = 0; SurfaceIdx < ModelLod.NumSurfs; ++SurfaceIdx)
		{
			FWraithXModelSubmesh& SubMeshRef = LodRef.Submeshes.AddDefaulted_GetRef();

			FMW5XSurface Surface;
			MemoryReader->ReadMemory<FMW5XSurface>(XSurfacePtr + SurfaceIdx * sizeof(FMW5XSurface), Surface);

			SubMeshRef.RigidWeightsPtr = Surface.RigidWeightsPtr;
			SubMeshRef.VertListcount = Surface.VertListCount;
			SubMeshRef.VertexCount = Surface.VertexCount;
			SubMeshRef.FaceCount = Surface.FacesCount;
			SubMeshRef.VertexOffset = Surface.VertexOffset;
			SubMeshRef.VertexUVsOffset = Surface.VertexUVsOffset;
			SubMeshRef.VertexTangentOffset = Surface.VertexTangentOffset;
			SubMeshRef.FacesOffset = Surface.FacesOffset;
			SubMeshRef.VertexColorOffset = Surface.VertexColorOffset;
			SubMeshRef.WeightsOffset = Surface.WeightsOffset;

			SubMeshRef.VertexSecondUVsOffset = 0xFFFFFFFF;

			if (Surface.NewScale != -1)
			{
				SubMeshRef.Scale = Surface.NewScale;
				SubMeshRef.XOffset = 0;
				SubMeshRef.YOffset = 0;
				SubMeshRef.ZOffset = 0;
			}
			else
			{
				SubMeshRef.Scale = fmaxf(fmaxf(Surface.Min, Surface.Scale), Surface.Max);
				SubMeshRef.XOffset = Surface.XOffset;
				SubMeshRef.YOffset = Surface.YOffset;
				SubMeshRef.ZOffset = Surface.ZOffset;
			}

			SubMeshRef.WeightCounts[0] = Surface.WeightCounts[0];
			SubMeshRef.WeightCounts[1] = Surface.WeightCounts[1];
			SubMeshRef.WeightCounts[2] = Surface.WeightCounts[2];
			SubMeshRef.WeightCounts[3] = Surface.WeightCounts[3];
			SubMeshRef.WeightCounts[4] = Surface.WeightCounts[4];
			SubMeshRef.WeightCounts[5] = Surface.WeightCounts[5];
			SubMeshRef.WeightCounts[6] = Surface.WeightCounts[6];
			SubMeshRef.WeightCounts[7] = Surface.WeightCounts[7];

			uint64 MaterialHandle;
			MemoryReader->ReadMemory<uint64>(ModelData.MaterialHandlesPtr + SurfaceIdx * sizeof(uint64),
			                                 MaterialHandle);

			FWraithXMaterial& Material = LodRef.Materials.AddDefaulted_GetRef();
			ReadMaterialDataFromPtr(MaterialHandle, Material);
		}
	}
	return true;
}

bool ModernWarfare5AssetHandler::ReadAnimData(TSharedPtr<FCoDAnim> AnimInfo, FWraithXAnim& OutAnim)
{
	FMW5XAnim AnimData;
	if (!MemoryReader->ReadMemory<FMW5XAnim>(AnimInfo->AssetPointer, AnimData))
	{
		UE_LOG(LogTemp, Error, TEXT("ReadAnimData: Failed to read FMW5XAnim at handle 0x%llX"), AnimInfo->AssetPointer);
		return false;
	}
	if (!AnimData.DataInfo.StreamInfoPtr) return false;
	OutAnim.AnimationName = AnimInfo->AssetName;
	OutAnim.FrameCount = AnimData.FrameCount;
	OutAnim.FrameRate = AnimData.Framerate;

	OutAnim.LoopingAnimation = (AnimData.Padding2[4] & 1);
	OutAnim.AdditiveAnimation = AnimData.AssetType == 0x6;

	OutAnim.NoneRotatedBoneCount = AnimData.NoneRotatedBoneCount;
	OutAnim.TwoDRotatedBoneCount = AnimData.TwoDRotatedBoneCount;
	OutAnim.NormalRotatedBoneCount = AnimData.NormalRotatedBoneCount;
	OutAnim.TwoDStaticRotatedBoneCount = AnimData.TwoDStaticRotatedBoneCount;
	OutAnim.NormalStaticRotatedBoneCount = AnimData.NormalStaticRotatedBoneCount;
	OutAnim.NormalTranslatedBoneCount = AnimData.NormalTranslatedBoneCount;
	OutAnim.PreciseTranslatedBoneCount = AnimData.PreciseTranslatedBoneCount;
	OutAnim.StaticTranslatedBoneCount = AnimData.StaticTranslatedBoneCount;
	OutAnim.NoneTranslatedBoneCount = AnimData.NoneTranslatedBoneCount;
	OutAnim.TotalBoneCount = AnimData.TotalBoneCount;
	OutAnim.NotificationCount = AnimData.NotetrackCount;
	OutAnim.ReaderInformationPointer = AnimInfo->AssetPointer;

	for (int32 BoneIdx = 0; BoneIdx < AnimData.TotalBoneCount; ++BoneIdx)
	{
		uint32 BoneNameHash;
		MemoryReader->ReadMemory<uint32>(AnimData.BoneIDsPtr + BoneIdx * 4, BoneNameHash);
		FString BoneName = FCoDDatabaseService::Get().GetPrintfAssetName(BoneNameHash, "Bone");
		OutAnim.Reader.BoneNames.Add(BoneName);
	}

	for (int32 NoteTrackIdx = 0; NoteTrackIdx < AnimData.NotetrackCount; ++NoteTrackIdx)
	{
		FMW5XAnimNoteTrack NoteTrack;
		MemoryReader->ReadMemory<FMW5XAnimNoteTrack>(
			AnimData.NotificationsPtr + NoteTrackIdx * sizeof(FMW5XAnimNoteTrack), NoteTrack);
		FString TrackName;
		MemoryReader->ReadString(GameProcess->ParasyteState->StringsAddress + NoteTrack.Name, TrackName);
		OutAnim.Reader.Notetracks.Emplace(TrackName, NoteTrack.Time * OutAnim.FrameCount);
	}

	FMW5XAnimDeltaParts AnimDeltaData;
	MemoryReader->ReadMemory<FMW5XAnimDeltaParts>(AnimData.DeltaPartsPtr, AnimDeltaData);

	OutAnim.DeltaTranslationPtr = AnimDeltaData.DeltaTranslationsPtr;
	OutAnim.Delta2DRotationsPtr = AnimDeltaData.Delta2DRotationsPtr;
	OutAnim.Delta3DRotationsPtr = AnimDeltaData.Delta3DRotationsPtr;

	OutAnim.RotationType = EAnimationKeyTypes::DivideBySize;
	OutAnim.TranslationType = EAnimationKeyTypes::MinSizeTable;

	OutAnim.SupportsInlineIndicies = true;

	return true;
}

bool ModernWarfare5AssetHandler::ReadImageData(TSharedPtr<FCoDImage> ImageInfo, TArray<uint8>& OutImageData,
                                               uint8& OutFormat)
{
	return ReadImageDataFromPtr(ImageInfo->AssetPointer, OutImageData, OutFormat);
}

bool ModernWarfare5AssetHandler::ReadImageDataFromPtr(uint64 ImageHandle, TArray<uint8>& OutImageData, uint8& OutFormat)
{
	TArray<uint8> RawPixelData;

	FMW5GfxImage ImageAsset;
	if (!MemoryReader->ReadMemory<FMW5GfxImage>(ImageHandle, ImageAsset))
	{
		UE_LOG(LogTemp, Error, TEXT("ReadImageDataFromPtr: Failed to read FMW6GfxImage at handle 0x%llX"), ImageHandle);
		return false;
	}
	OutFormat = GMW5DXGIFormats[ImageAsset.ImageFormat];
	uint32 ImageSize = 0;
	uint32 ActualWidth;
	uint32 ActualHeight;
	if (ImageAsset.LoadedImagePtr)
	{
		ImageSize = ImageAsset.BufferSize;
		if (ImageSize == 0)
		{
			UE_LOG(LogTemp, Error, TEXT("ReadImageDataFromPtr: BufferSize is 0 for LoadedImagePtr case. Hash: 0x%llX"),
			       ImageAsset.Hash);
			return false;
		}

		RawPixelData.SetNumUninitialized(ImageSize);
		if (!MemoryReader->ReadArray(ImageAsset.LoadedImagePtr, RawPixelData, ImageAsset.BufferSize) || RawPixelData.
			Num() != ImageSize)
		{
			RawPixelData.Empty();
			UE_LOG(LogTemp, Error,
			       TEXT(
				       "ReadImageDataFromPtr: Failed to read %d bytes from LoadedImagePtr 0x%llX. Read %d bytes. Hash: 0x%llX"
			       ), ImageSize, ImageAsset.LoadedImagePtr, RawPixelData.Num(), ImageAsset.Hash);
			return false;
		}
		ActualWidth = ImageAsset.Width;
		ActualHeight = ImageAsset.Height;
	}
	else
	{
		if (ImageAsset.MipMaps == 0)
		{
			UE_LOG(LogTemp, Error,
			       TEXT(
				       "ReadImageDataFromPtr: MipMaps pointer is null and LoadedImagePtr is null. Cannot get data. Hash: 0x%llX"
			       ), ImageAsset.Hash);
			return false;
		}

		FMW5GfxMipArray Mips;
		uint32 MipCount = FMath::Min(static_cast<uint32>(ImageAsset.MipCount), 32u);
		if (!MemoryReader->ReadArray(ImageAsset.MipMaps, Mips.MipMaps, MipCount))
		{
			UE_LOG(LogTemp, Error, TEXT("ReadImageDataFromPtr: Failed to read MipMap array structure. Hash: 0x%llX"),
			       ImageAsset.Hash);
			return false;
		}

		int32 FallbackMipIndex = 0;
		int32 HighestIndex = ImageAsset.MipCount - 1;

		for (uint32 MipIdx = 0; MipIdx < MipCount; ++MipIdx)
		{
			if (GameProcess->GetDecrypt()->ExistsKey(Mips.MipMaps[MipIdx].HashID))
			{
				FallbackMipIndex = MipIdx;
			}
		}

		if (FallbackMipIndex != HighestIndex && GameProcess && GameProcess->GetCDNDownloader())
		{
			ImageSize =
				GameProcess->GetCDNDownloader()->ExtractCDNObject(RawPixelData,
				                                                  Mips.MipMaps[HighestIndex].HashID,
				                                                  Mips.GetImageSize(HighestIndex));
		}

		if (RawPixelData.IsEmpty())
		{
			RawPixelData = GameProcess->GetDecrypt()->ExtractXSubPackage(Mips.MipMaps[FallbackMipIndex].HashID,
			                                                             Mips.GetImageSize(FallbackMipIndex));
			ImageSize = RawPixelData.Num();
			HighestIndex = FallbackMipIndex;
		}
		ActualWidth = ImageAsset.Width >> (MipCount - HighestIndex - 1);
		ActualHeight = ImageAsset.Height >> (MipCount - HighestIndex - 1);
	}
	TArray<uint8> DDSHeader = FCoDAssetHelper::BuildDDSHeader(ActualWidth, ActualHeight, 1, 1, OutFormat, false);

	if (DDSHeader.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("ReadImageDataFromPtr: Failed to build DDS header (MipCount=1) for hash 0x%llX"),
		       ImageAsset.Hash);
		return false;
	}

	OutImageData.Empty(DDSHeader.Num() + RawPixelData.Num());
	OutImageData.Append(DDSHeader);
	OutImageData.Append(MoveTemp(RawPixelData));
	return true;
}

bool ModernWarfare5AssetHandler::ReadImageDataToTexture(uint64 ImageHandle, UTexture2D*& OutTexture, FString& ImageName,
                                                        FString ImagePath)
{
	FMW5GfxImage ImageAsset;
	if (!MemoryReader->ReadMemory<FMW5GfxImage>(ImageHandle, ImageAsset))
	{
		return false;
	}
	if (ImageAsset.Width < 2 && ImageAsset.Height < 2)
	{
		return false;
	}
	if (ImageName.IsEmpty())
	{
		ImageName = FCoDDatabaseService::Get().GetPrintfAssetName(ImageAsset.Hash, TEXT("ximage"));
		ImageName = FCoDAssetNameHelper::NoIllegalSigns(ImageName);
	}
	TArray<uint8> CompleteDDSData;
	uint8 DxgiFormat;
	if (!ReadImageDataFromPtr(ImageHandle, CompleteDDSData, DxgiFormat))
	{
		UE_LOG(LogTemp, Error, TEXT("ReadImageDataToTexture: Failed to read image data for handle 0x%llX (Name: %s)"),
		       ImageHandle, *ImageName);
		return false;
	}
	if (CompleteDDSData.IsEmpty())
	{
		UE_LOG(LogTemp, Error,
		       TEXT("ReadImageDataToTexture: ReadImageDataFromPtr returned success but data array is empty for %s"),
		       *ImageName);
		return false;
	}
	OutTexture = FCoDAssetHelper::CreateTextureFromDDSData(CompleteDDSData, ImageName, ImagePath);
	return true;
}

bool ModernWarfare5AssetHandler::ReadSoundData(TSharedPtr<FCoDSound> SoundInfo, FWraithXSound& OutSound)
{
	FMW5SoundAsset SoundData;
	if (!MemoryReader->ReadMemory<FMW5SoundAsset>(SoundInfo->AssetPointer, SoundData))
	{
		return false;
	}
	OutSound.ChannelCount = SoundData.ChannelCount;
	OutSound.FrameCount = SoundData.FrameCount;
	OutSound.FrameRate = SoundData.FrameRate;
	if (SoundData.StreamKey)
	{
		TArray<uint8> SoundBuffer = GameProcess->GetDecrypt()->ExtractXSubPackage(
			SoundData.StreamKey, ((SoundData.Size + SoundData.SeekTableSize + 4095) & 0xFFFFFFFFFFFFFFF0));
		if (SoundBuffer.IsEmpty()) return false;
		SoundBuffer.RemoveAt(0, SoundData.SeekTableSize);
		return SABSupport::DecodeOpusInterLeaved(OutSound, SoundBuffer,
		                                         0, SoundData.FrameRate,
		                                         SoundData.ChannelCount, SoundData.FrameCount);
	}
	TArray<uint8> SoundBuffer = GameProcess->GetDecrypt()->ExtractXSubPackage(
		SoundData.StreamKeyEx, ((SoundData.LoadedSize + SoundData.SeekTableSize + 4095) & 0xFFFFFFFFFFFFFFF0));
	if (SoundBuffer.IsEmpty()) return false;
	SoundBuffer.RemoveAt(0, 32 + SoundData.SeekTableSize);
	return SABSupport::DecodeOpusInterLeaved(OutSound, SoundBuffer,
	                                         0, SoundData.FrameRate,
	                                         SoundData.ChannelCount, SoundData.FrameCount);
}

bool ModernWarfare5AssetHandler::ReadMaterialData(TSharedPtr<FCoDMaterial> MaterialInfo, FWraithXMaterial& OutMaterial)
{
	return ReadMaterialDataFromPtr(MaterialInfo->AssetPointer, OutMaterial);
}

bool ModernWarfare5AssetHandler::ReadMaterialDataFromPtr(uint64 MaterialHandle, FWraithXMaterial& OutMaterial)
{
	FMW5Material MaterialData;
	MemoryReader->ReadMemory<FMW5Material>(MaterialHandle, MaterialData);

	OutMaterial.MaterialPtr = MaterialHandle;
	OutMaterial.MaterialHash = (MaterialData.Hash & 0xFFFFFFFFFFFFFFF);
	OutMaterial.MaterialName = FCoDDatabaseService::Get().GetPrintfAssetName(MaterialData.Hash, TEXT("xmaterial"));

	for (uint32 i = 0; i < MaterialData.ImageCount; i++)
	{
		FMW5XMaterialImage ImageInfo;
		MemoryReader->ReadMemory(MaterialData.ImageTable + i * sizeof(FMW5XMaterialImage), ImageInfo);
		uint64 ImageNameHash;
		MemoryReader->ReadMemory(ImageInfo.ImagePtr, ImageNameHash);

		FString ImageName = FCoDDatabaseService::Get().GetPrintfAssetName(ImageNameHash, TEXT("ximage"));

		if (!ImageName.IsEmpty() && ImageName[0] != '$')
		{
			FWraithXImage& ImageOut = OutMaterial.Images.AddDefaulted_GetRef();

			ImageOut.ImagePtr = ImageInfo.ImagePtr;
			ImageOut.ImageName = ImageName;
			ImageOut.SemanticHash = ImageInfo.Type;
		}
	}
	return true;
}

bool ModernWarfare5AssetHandler::ReadMapData(TSharedPtr<FCoDMap> MapInfo, FWraithXMap& OutMapData)
{
	return false;
}

bool ModernWarfare5AssetHandler::TranslateAnim(const FWraithXAnim& InAnim, FCastAnimationInfo& OutAnimInfo)
{
	return false;
}

bool ModernWarfare5AssetHandler::LoadStreamedModelData(const FWraithXModel& InModel, FWraithXModelLod& InOutLod,
                                                       FCastModelInfo& OutModelInfo)
{
	return false;
}

bool ModernWarfare5AssetHandler::LoadStreamedAnimData(const FWraithXAnim& InAnim, FCastAnimationInfo& OutAnimInfo)
{
	return false;
}

void ModernWarfare5AssetHandler::LoadXModel(FWraithXModel& InModel, FWraithXModelLod& ModelLod,
                                            FCastModelInfo& OutModel)
{
	FMW5XModelMesh MeshInfo;
	MemoryReader->ReadMemory(ModelLod.LODStreamInfoPtr, MeshInfo);
	FMW5XModelMeshBufferInfo BufferInfo;
	MemoryReader->ReadMemory(MeshInfo.MeshBufferPointer, BufferInfo);

	TArray<uint8> MeshDataBuffer;
	if (BufferInfo.BufferPtr)
	{
		MemoryReader->ReadArray(BufferInfo.BufferPtr, MeshDataBuffer, BufferInfo.BufferSize);
	}
	else
	{
		MeshDataBuffer = GameProcess->GetDecrypt()->ExtractXSubPackage(MeshInfo.LODStreamKey, BufferInfo.BufferSize);
	}

	CoDBonesHelper::ReadXModelBones(MemoryReader, InModel, ModelLod, OutModel);

	if (MeshDataBuffer.IsEmpty())
	{
		return;
	}

	for (FWraithXModelSubmesh& Submesh : ModelLod.Submeshes)
	{
		FCastMeshInfo& Mesh = OutModel.Meshes.AddDefaulted_GetRef();
		Mesh.MaterialIndex = Submesh.MaterialIndex;
		Mesh.MaterialHash = Submesh.MaterialHash;

		FBufferReader VertexPosReader(MeshDataBuffer.GetData() + Submesh.VertexOffset,
		                              Submesh.VertexCount * sizeof(uint64), false);
		FBufferReader VertexTangentReader(MeshDataBuffer.GetData() + Submesh.VertexTangentOffset,
		                                  Submesh.VertexCount * sizeof(uint32), false);
		FBufferReader VertexUVReader(MeshDataBuffer.GetData() + Submesh.VertexUVsOffset,
		                             Submesh.VertexCount * sizeof(uint32), false);
		FBufferReader SecondUVReader(MeshDataBuffer.GetData() + Submesh.VertexSecondUVsOffset,
		                             Submesh.VertexCount * sizeof(uint32), false);
		FBufferReader FaceIndiciesReader(MeshDataBuffer.GetData() + Submesh.FacesOffset,
		                                 Submesh.FaceCount * sizeof(uint16) * 3, false);


		bool bHasUV0 = (Submesh.VertexUVsOffset != 0 && Submesh.VertexUVsOffset != 0xFFFFFFFF);
		bool bHasUV1 = (Submesh.VertexSecondUVsOffset != 0 && Submesh.VertexSecondUVsOffset != 0xFFFFFFFF);

		if (bHasUV1)
		{
			Mesh.VertexUVs.SetNum(2);
		}
		else if (bHasUV0)
		{
			Mesh.VertexUVs.SetNum(1);
		}
		else
		{
			Mesh.VertexUVs.SetNum(0);
		}

		// 顶点权重
		Mesh.VertexWeights.SetNum(Submesh.VertexCount);
		int32 WeightDataLength = 0;
		for (int32 WeightIdx = 0; WeightIdx < 8; ++WeightIdx)
		{
			WeightDataLength += (WeightIdx + 1) * 4 * Submesh.WeightCounts[WeightIdx];
		}
		if (OutModel.Skeletons.Num() > 0)
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

		if (Submesh.VertexSecondUVsOffset != 0)
		{
			Mesh.VertexUVs.SetNum(2);
		}
		else
		{
			Mesh.VertexUVs.SetNum(1);
		}

		Mesh.VertexColor.Reserve(Submesh.VertexCount);

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
			FCoDMeshHelper::UnpackCoDQTangent(PackedTangentFrame, Tangent, Normal);
			Mesh.VertexNormals.Add(Normal);
			Mesh.VertexTangents.Add(Tangent);

			auto ReadUV = [](FBufferReader& InReader, FVector2f& OutUV)
			{
				uint16 HalfUvU, HalfUvV;
				InReader << HalfUvU << HalfUvV;
				OutUV.X = HalfFloatHelper::ToFloat(HalfUvU);
				OutUV.Y = HalfFloatHelper::ToFloat(HalfUvV);
			};

			if (bHasUV0 && Mesh.VertexUVs.IsValidIndex(0))
			{
				FVector2f UV;
				ReadUV(VertexUVReader, UV);
				Mesh.VertexUVs[0].Add(MoveTemp(UV));
			}

			if (bHasUV1 && Mesh.VertexUVs.IsValidIndex(1))
			{
				FVector2f UV;
				ReadUV(SecondUVReader, UV);
				Mesh.VertexUVs[1].Add(MoveTemp(UV));
			}
		}
		// 顶点色
		if (Submesh.VertexColorOffset != 0xFFFFFFFF)
		{
			FBufferReader VertexColorReader(MeshDataBuffer.GetData() + Submesh.VertexColorOffset,
			                                Submesh.VertexCount * sizeof(uint32), false);
			for (uint32 VertexIdx = 0; VertexIdx < Submesh.VertexCount; VertexIdx++)
			{
				uint32 VertexColor;
				VertexColorReader << VertexColor;
				Mesh.VertexColor.Add(VertexColor);
			}
		}
		// 面

		for (uint32 TriIdx = 0; TriIdx < Submesh.FaceCount; TriIdx++)
		{
			uint16 FaceIdx1, FaceIdx2, FaceIdx3;
			FaceIndiciesReader << FaceIdx1 << FaceIdx2 << FaceIdx3;
			Mesh.Faces.Add(FaceIdx3);
			Mesh.Faces.Add(FaceIdx2);
			Mesh.Faces.Add(FaceIdx1);
		}
	}
}
