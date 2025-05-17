#include "GameInfo/ModernWarfare6AssetHandler.h"

#include "CastManager/CastRoot.h"
#include "CDN/CoDCDNDownloader.h"
#include "Database/CoDDatabaseService.h"
#include "GameInfo/ModernWarfare6.h"
#include "Interface/IMemoryReader.h"
#include "Structures/MW6SPGameStructures.h"
#include "ThirdSupport/SABSupport.h"
#include "Utils/CoDAssetHelper.h"
#include "Utils/CoDBonesHelper.h"
#include "WraithX/LocateGameInfo.h"

bool FModernWarfare6AssetHandler::ReadModelData(TSharedPtr<FCoDModel> ModelInfo, FWraithXModel& OutModel)
{
	FMW6XModel ModelData;
	MemoryReader->ReadMemory<FMW6XModel>(ModelInfo->AssetPointer, ModelData);
	FMW6BoneInfo BoneInfo;
	MemoryReader->ReadMemory<FMW6BoneInfo>(ModelData.BoneInfoPtr, BoneInfo);

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

	if (BoneInfo.BoneParentsPtr)
	{
		OutModel.BoneCount = BoneInfo.NumBones + BoneInfo.CosmeticBoneCount;
		OutModel.RootBoneCount = BoneInfo.NumRootBones;
	}

	OutModel.BoneRotationData = EBoneDataTypes::DivideBySize;
	OutModel.IsModelStreamed = true;
	OutModel.BoneIDsPtr = BoneInfo.BoneIDsPtr;
	OutModel.BoneIndexSize = sizeof(uint32);
	OutModel.BoneParentsPtr = BoneInfo.BoneParentsPtr;
	OutModel.BoneParentSize = sizeof(uint16);
	OutModel.RotationsPtr = BoneInfo.RotationsPtr;
	OutModel.TranslationsPtr = BoneInfo.TranslationsPtr;
	OutModel.BaseTransformPtr = BoneInfo.BaseMatriciesPtr;

	for (uint32 LodIdx = 0; LodIdx < ModelData.NumLods; ++LodIdx)
	{
		FMW6XModelLod ModelLod;
		MemoryReader->ReadMemory<FMW6XModelLod>(ModelData.LodInfo + LodIdx * sizeof(FMW6XModelLod), ModelLod);

		FWraithXModelLod& LodRef = OutModel.ModelLods.AddDefaulted_GetRef();

		LodRef.LodDistance = ModelLod.LodDistance;
		LodRef.LODStreamInfoPtr = ModelLod.MeshPtr;
		uint64 XSurfacePtr = ModelLod.SurfsPtr;

		LodRef.Submeshes.Reserve(ModelLod.NumSurfs);
		for (uint32 SurfaceIdx = 0; SurfaceIdx < ModelLod.NumSurfs; ++SurfaceIdx)
		{
			FWraithXModelSubmesh& SubMeshRef = LodRef.Submeshes.AddDefaulted_GetRef();

			FMW6XSurface Surface;
			MemoryReader->ReadMemory<FMW6XSurface>(XSurfacePtr + SurfaceIdx * sizeof(FMW6XSurface), Surface);

			SubMeshRef.VertexCount = Surface.VertCount;
			SubMeshRef.FaceCount = Surface.TriCount;
			SubMeshRef.PackedIndexTableCount = Surface.PackedIndicesTableCount;
			SubMeshRef.VertexOffset = Surface.XyzOffset;
			SubMeshRef.VertexUVsOffset = Surface.TexCoordOffset;
			SubMeshRef.VertexTangentOffset = Surface.TangentFrameOffset;
			SubMeshRef.FacesOffset = Surface.IndexDataOffset;
			SubMeshRef.PackedIndexTableOffset = Surface.PackedIndiciesTableOffset;
			SubMeshRef.PackedIndexBufferOffset = Surface.PackedIndicesOffset;
			SubMeshRef.VertexColorOffset = Surface.VertexColorOffset;
			SubMeshRef.WeightsOffset = Surface.WeightsOffset;

			if (Surface.OverrideScale != -1)
			{
				SubMeshRef.Scale = Surface.OverrideScale;
				SubMeshRef.XOffset = 0;
				SubMeshRef.YOffset = 0;
				SubMeshRef.ZOffset = 0;
			}
			else
			{
				SubMeshRef.Scale = FMath::Max3(Surface.Min, Surface.Max, Surface.Scale);
				SubMeshRef.XOffset = Surface.OffsetsX;
				SubMeshRef.YOffset = Surface.OffsetsY;
				SubMeshRef.ZOffset = Surface.OffsetsZ;
			}
			for (int32 WeightIdx = 0; WeightIdx < 8; ++WeightIdx)
			{
				SubMeshRef.WeightCounts[WeightIdx] = Surface.WeightCounts[WeightIdx];
			}

			uint64 MaterialHandle;
			MemoryReader->ReadMemory<uint64>(ModelData.MaterialHandles + SurfaceIdx * sizeof(uint64), MaterialHandle);

			FWraithXMaterial& Material = LodRef.Materials.AddDefaulted_GetRef();
			ReadMaterialDataFromPtr(MaterialHandle, Material);
		}
	}

	return true;
}

bool FModernWarfare6AssetHandler::ReadAnimData(TSharedPtr<FCoDAnim> AnimInfo, FWraithXAnim& OutAnim)
{
	FMW6XAnim AnimData;
	if (!MemoryReader->ReadMemory<FMW6XAnim>(AnimInfo->AssetPointer, AnimData))
	{
		UE_LOG(LogTemp, Error, TEXT("ReadAnimData: Failed to read FMW6XAnim at handle 0x%llX"), AnimInfo->AssetPointer);
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
		FMW6XAnimNoteTrack NoteTrack;
		MemoryReader->ReadMemory<FMW6XAnimNoteTrack>(
			AnimData.NotificationsPtr + NoteTrackIdx * sizeof(FMW6XAnimNoteTrack), NoteTrack);
		FString TrackName;
		// MemoryReader->Sta
		MemoryReader->ReadString(GameProcess->ParasyteState->StringsAddress + NoteTrack.Name, TrackName);
		OutAnim.Reader.Notetracks.Emplace(TrackName, NoteTrack.Time * OutAnim.FrameCount);
	}

	FMW6XAnimDeltaParts AnimDeltaData;
	MemoryReader->ReadMemory<FMW6XAnimDeltaParts>(AnimData.DeltaPartsPtr, AnimDeltaData);

	OutAnim.DeltaTranslationPtr = AnimDeltaData.DeltaTranslationsPtr;
	OutAnim.Delta2DRotationsPtr = AnimDeltaData.Delta2DRotationsPtr;
	OutAnim.Delta3DRotationsPtr = AnimDeltaData.Delta3DRotationsPtr;

	OutAnim.RotationType = EAnimationKeyTypes::DivideBySize;
	OutAnim.TranslationType = EAnimationKeyTypes::MinSizeTable;

	OutAnim.SupportsInlineIndicies = true;

	return true;
}

bool FModernWarfare6AssetHandler::ReadImageData(TSharedPtr<FCoDImage> ImageInfo, TArray<uint8>& OutImageData,
                                                uint8& OutFormat)
{
	return ReadImageDataFromPtr(ImageInfo->AssetPointer, OutImageData, OutFormat);
}

bool FModernWarfare6AssetHandler::ReadImageDataFromPtr(uint64 ImageHandle, TArray<uint8>& OutCompleteDDSData,
                                                       uint8& OutDxgiFormat)
{
	TArray<uint8> RawPixelData;

	FMW6GfxImage ImageAsset;
	if (!MemoryReader->ReadMemory<FMW6GfxImage>(ImageHandle, ImageAsset))
	{
		UE_LOG(LogTemp, Error, TEXT("ReadImageDataFromPtr: Failed to read FMW6GfxImage at handle 0x%llX"), ImageHandle);
		return false;
	}
	OutDxgiFormat = GMW6DXGIFormats[ImageAsset.ImageFormat];
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
				       "ReadImageDataFromPtr: Failed to read %d bytes from LoadedImagePtr 0x%llX. Read %llu bytes. Hash: 0x%llX"
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

		FMW6GfxMipArray Mips;
		uint32 MipCount = FMath::Min(static_cast<uint32>(ImageAsset.MipCount), 32u);
		if (!MemoryReader->ReadArray(ImageAsset.MipMaps, Mips.MipMaps, MipCount))
		{
			UE_LOG(LogTemp, Error, TEXT("ReadImageDataFromPtr: Failed to read MipMap array structure. Hash: 0x%llX"),
			       ImageAsset.Hash);
			return false;
		}

		int32 FallbackMipIndex = -1;
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

	TArray<uint8> DDSHeader = FCoDAssetHelper::BuildDDSHeader(ActualWidth, ActualHeight, 1, 1, OutDxgiFormat, false);

	if (DDSHeader.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("ReadImageDataFromPtr: Failed to build DDS header (MipCount=1) for hash 0x%llX"),
		       ImageAsset.Hash);
		return false;
	}

	OutCompleteDDSData.Empty(DDSHeader.Num() + RawPixelData.Num());
	OutCompleteDDSData.Append(DDSHeader);
	OutCompleteDDSData.Append(MoveTemp(RawPixelData));

	return true;
}

bool FModernWarfare6AssetHandler::ReadImageDataToTexture(uint64 ImageHandle, UTexture2D*& OutTexture,
                                                         FString& ImageName, FString ImagePath)
{
	FMW6GfxImage ImageAsset;
	if (!MemoryReader->ReadMemory<FMW6GfxImage>(ImageHandle, ImageAsset))
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

bool FModernWarfare6AssetHandler::ReadSoundData(TSharedPtr<FCoDSound> SoundInfo, FWraithXSound& OutSound)
{
	FMW6SoundAsset SoundData;
	if (!MemoryReader->ReadMemory<FMW6SoundAsset>(SoundInfo->AssetPointer, SoundData))
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

bool FModernWarfare6AssetHandler::ReadMaterialData(TSharedPtr<FCoDMaterial> MaterialInfo, FWraithXMaterial& OutMaterial)
{
	return ReadMaterialDataFromPtr(MaterialInfo->AssetPointer, OutMaterial);
}

bool FModernWarfare6AssetHandler::ReadMaterialDataFromPtr(uint64 MaterialHandle, FWraithXMaterial& OutMaterial)
{
	if (GameProcess->GetCurrentGameFlag() == CoDAssets::ESupportedGameFlags::SP)
	{
		FMW6SPMaterial MaterialData;
		MemoryReader->ReadMemory<FMW6SPMaterial>(MaterialHandle, MaterialData);
	}
	else
	{
		FMW6Material MaterialData;
		MemoryReader->ReadMemory<FMW6Material>(MaterialHandle, MaterialData);

		OutMaterial.MaterialPtr = MaterialHandle;
		OutMaterial.MaterialHash = (MaterialData.Hash & 0xFFFFFFFFFFFFFFF);
		OutMaterial.MaterialName = FCoDDatabaseService::Get().GetPrintfAssetName(MaterialData.Hash, TEXT("xmaterial"));

		struct FImageWithPtr
		{
			FMW6GfxImage Image;
			uint64 ImagePtr;
		};

		TArray<FImageWithPtr> Images;
		Images.Reserve(MaterialData.ImageCount);
		for (uint32 i = 0; i < MaterialData.ImageCount; i++)
		{
			uint64 ImagePtr;
			MemoryReader->ReadMemory<uint64>(MaterialData.ImageTable + i * sizeof(uint64), ImagePtr);
			FImageWithPtr ImageWithPtr;
			ImageWithPtr.ImagePtr = ImagePtr;
			MemoryReader->ReadMemory<FMW6GfxImage>(ImagePtr, ImageWithPtr.Image);
			Images.Add(ImageWithPtr);
		}

		OutMaterial.Images.Empty();

		for (int i = 0; i < MaterialData.TextureCount; i++)
		{
			FMW6MaterialTextureDef TextureDef;
			MemoryReader->ReadMemory<FMW6MaterialTextureDef>(
				MaterialData.TextureTable + i * sizeof(FMW6MaterialTextureDef), TextureDef);
			FMW6GfxImage Image = Images[TextureDef.ImageIdx].Image;

			FString ImageName;

			ImageName = FCoDDatabaseService::Get().GetPrintfAssetName(Image.Hash, TEXT("ximage"));

			// 一些占位符之类的，没有用
			if (!ImageName.IsEmpty() && ImageName[0] != '$')
			{
				FWraithXImage& ImageOut = OutMaterial.Images.AddDefaulted_GetRef();
				ImageOut.ImageName = ImageName;
				ImageOut.SemanticHash = TextureDef.Index;
				ImageOut.ImagePtr = Images[TextureDef.ImageIdx].ImagePtr;
			}
		}
	}
	return true;
}

bool FModernWarfare6AssetHandler::ReadMapData(TSharedPtr<FCoDMap> MapInfo, FWraithXMap& OutMapData)
{
	if (!MapInfo.IsValid() || !MemoryReader.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("ReadMapData failed: Invalid MapInfo or MemoryReader."));
		return false;
	}

	FMW6GfxWorld WorldData;
	if (!MemoryReader->ReadMemory<FMW6GfxWorld>(MapInfo->AssetPointer, WorldData))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to read FMW6GfxWorld structure."));
		return false;
	}

	OutMapData.MapName = FPaths::GetBaseFilename(MapInfo->AssetName);

	// --- Read Transient Zones ---
	TArray<FMW6GfxWorldTransientZone> TransientZones;
	TransientZones.SetNum(WorldData.TransientZoneCount);
	for (uint32 i = 0; i < WorldData.TransientZoneCount; i++)
	{
		if (!MemoryReader->ReadMemory<FMW6GfxWorldTransientZone>(WorldData.TransientZones[i], TransientZones[i]))
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to read Transient Zone %d."), i);
			return false;
		}
	}

	// --- Process Map Meshes (Surfaces) ---
	FMW6GfxWorldSurfaces& Surfaces = WorldData.Surfaces;
	OutMapData.MapMeshes.Reserve(Surfaces.Count);

	for (uint32 SurfaceIdx = 0; SurfaceIdx < Surfaces.Count; ++SurfaceIdx)
	{
		FMW6GfxSurface GfxSurface;
		if (!MemoryReader->ReadMemory<FMW6GfxSurface>(Surfaces.Surfaces + SurfaceIdx * sizeof(FMW6GfxSurface),
		                                              GfxSurface))
			continue;

		FMW6GfxUgbSurfData UgbSurfData;
		if (!MemoryReader->ReadMemory<FMW6GfxUgbSurfData>(
			Surfaces.UgbSurfData + GfxSurface.UgbSurfDataIndex * sizeof(FMW6GfxUgbSurfData), UgbSurfData))
			continue;

		if (!TransientZones.IsValidIndex(UgbSurfData.TransientZoneIndex)) continue;
		const FMW6GfxWorldTransientZone& Zone = TransientZones[UgbSurfData.TransientZoneIndex];
		if (Zone.Hash == 0 || GfxSurface.VertexCount == 0 || GfxSurface.TriCount == 0) continue;

		// --- Material Handling ---
		uint64 MaterialPtr;
		if (!MemoryReader->ReadMemory<uint64>(Surfaces.Materials + GfxSurface.MaterialIndex * 8, MaterialPtr)) continue;
		FMW6Material MaterialData;
		MemoryReader->ReadMemory<FMW6Material>(MaterialPtr, MaterialData);

		// --- Create Map Mesh Chunk Data ---
		FWraithMapMeshData& MeshChunk = OutMapData.MapMeshes.AddDefaulted_GetRef();
		MeshChunk.MeshName = FString::Printf(TEXT("%s_MeshChunk_%d"), *OutMapData.MapName, SurfaceIdx);
		FCastMeshInfo& MeshInfo = MeshChunk.MeshData;
		MeshInfo.Name = MeshChunk.MeshName;
		MeshInfo.MaterialHash = (MaterialData.Hash & 0xFFFFFFFFFFFFFFF);
		MeshInfo.MaterialPtr = MaterialPtr;

		// --- Read Vertex Data ---
		uint16 VertexCount = GfxSurface.VertexCount;
		MeshInfo.UVLayer = UgbSurfData.LayerCount > 0 ? UgbSurfData.LayerCount - 1 : 0;
		MeshInfo.VertexPositions.Reserve(VertexCount);
		MeshInfo.VertexNormals.Reserve(VertexCount);
		MeshInfo.VertexTangents.Reserve(VertexCount);

		MeshInfo.VertexUVs.SetNum(UgbSurfData.LayerCount);
		for (uint32 i = 0; i < UgbSurfData.LayerCount; i++)
		{
			MeshInfo.VertexUVs[i].Reserve(VertexCount);
		}

		if (UgbSurfData.ColorOffset != 0) MeshInfo.VertexColor.Reserve(VertexCount);

		uint64 XyzPtr = Zone.DrawVerts.PosData + UgbSurfData.XyzOffset;
		uint64 TangentFramePtr = Zone.DrawVerts.PosData + UgbSurfData.TangentFrameOffset;
		uint64 TexCoordPtr = Zone.DrawVerts.PosData + UgbSurfData.TexCoordOffset;
		uint64 ColorPtr = (UgbSurfData.ColorOffset != 0) ? (Zone.DrawVerts.PosData + UgbSurfData.ColorOffset) : 0;

		FMW6GfxWorldDrawOffset WorldDrawOffset = UgbSurfData.WorldDrawOffset;


		for (uint16 VertexIdx = 0; VertexIdx < VertexCount; ++VertexIdx)
		{
			uint64 PackedPosition;
			if (!MemoryReader->ReadMemory<uint64>(XyzPtr + VertexIdx * sizeof(uint64), PackedPosition)) continue;
			FVector3f Position{
				((PackedPosition >> 0) & 0x1FFFFF) * WorldDrawOffset.Scale + WorldDrawOffset.X,
				((PackedPosition >> 21) & 0x1FFFFF) * WorldDrawOffset.Scale + WorldDrawOffset.Y,
				((PackedPosition >> 42) & 0x1FFFFF) * WorldDrawOffset.Scale + WorldDrawOffset.Z
			};
			MeshInfo.VertexPositions.Add(Position);

			// Tangent Frame (Normal/Tangent)
			uint32 PackedTangentFrame;
			if (!MemoryReader->ReadMemory<uint32>(TangentFramePtr + VertexIdx * sizeof(uint32), PackedTangentFrame))
				continue;
			FVector3f Tangent, Normal;
			FCoDMeshHelper::UnpackCoDQTangent(PackedTangentFrame, Tangent, Normal);
			MeshInfo.VertexTangents.Add(Tangent);
			MeshInfo.VertexNormals.Add(Normal);

			// UVs
			for (uint32 LayerIdx = 0; LayerIdx < UgbSurfData.LayerCount; ++LayerIdx)
			{
				uint64 CurrentLayerUVPtr = TexCoordPtr + sizeof(FVector2f) * (VertexIdx * UgbSurfData.LayerCount +
					LayerIdx);
				FVector2f LayerUV;
				MemoryReader->ReadMemory<FVector2f>(CurrentLayerUVPtr, LayerUV);
				MeshInfo.VertexUVs[LayerIdx].Add(MoveTemp(LayerUV));
			}

			// Color
			if (ColorPtr != 0)
			{
				// uint32 PackedColor;
				// if (!MemoryReader->ReadMemory<uint32>(ColorPtr + VertexIdx * sizeof(uint32), PackedColor)) continue;
				// MeshInfo.VertexColor.Add(PackedColor);
			}
		}

		// --- Read Face Indices ---
		uint64 TableOffsetPtr = Zone.DrawVerts.TableData + GfxSurface.TableIndex * 40;
		uint64 IndicesPtr = Zone.DrawVerts.Indices + GfxSurface.BaseIndex * sizeof(uint16);
		uint64 PackedIndicesPtr = Zone.DrawVerts.PackedIndices + GfxSurface.PackedIndicesOffset;

		MeshInfo.Faces.Reserve(GfxSurface.TriCount * 3);

		for (int TriIndex = 0; TriIndex < GfxSurface.TriCount; ++TriIndex)
		{
			TArray<uint16> FaceIndices;
			if (!FCoDMeshHelper::UnpackFaceIndices(MemoryReader,
			                                       FaceIndices, TableOffsetPtr,
			                                       GfxSurface.PackedIndicesTableCount,
			                                       PackedIndicesPtr, IndicesPtr, TriIndex, false))
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to unpack faces for surface %d, triangle %d."), SurfaceIdx,
				       TriIndex);
				MeshInfo.Faces.SetNum(0);
				break;
			}

			if (FaceIndices.Num() == 3)
			{
				MeshInfo.Faces.Add(FaceIndices[2]);
				MeshInfo.Faces.Add(FaceIndices[1]);
				MeshInfo.Faces.Add(FaceIndices[0]);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("UnpackFaceIndices returned %d indices for triangle %d (expected 3)."),
				       FaceIndices.Num(), TriIndex);
				MeshInfo.Faces.SetNum(0);
				break;
			}
		}

		if (MeshInfo.Faces.Num() != GfxSurface.TriCount * 3)
		{
			OutMapData.MapMeshes.Pop();
			UE_LOG(LogTemp, Error, TEXT("Removed invalid mesh chunk %s due to face reading errors."),
			       *MeshChunk.MeshName);
		}
		else
		{
			UE_LOG(LogTemp, Verbose, TEXT("Processed map mesh chunk %s: Verts=%d, Tris=%d"), *MeshChunk.MeshName,
			       MeshInfo.VertexPositions.Num(), MeshInfo.Faces.Num() / 3);
		}
	}

	// --- Process Static Model Instances ---
	UE_LOG(LogTemp, Log, TEXT("Processing %d static model collections..."), WorldData.SModels.CollectionsCount);
	FMW6GfxWorldStaticModels& SModels = WorldData.SModels;
	OutMapData.StaticModelInstances.Reserve(SModels.InstanceCount);

	for (uint32 CollectionIdx = 0; CollectionIdx < SModels.CollectionsCount; ++CollectionIdx)
	{
		FMW6GfxStaticModelCollection StaticModelCollection;
		if (!MemoryReader->ReadMemory(SModels.Collections + CollectionIdx * sizeof(FMW6GfxStaticModelCollection),
		                              StaticModelCollection))
			continue;

		FMW6GfxStaticModel StaticModel;
		if (!MemoryReader->ReadMemory(SModels.SModels + StaticModelCollection.SModelIndex * sizeof(FMW6GfxStaticModel),
		                              StaticModel))
			continue;

		if (!TransientZones.IsValidIndex(StaticModelCollection.TransientGfxWorldPlaced)) continue;
		const FMW6GfxWorldTransientZone& Zone = TransientZones[StaticModelCollection.TransientGfxWorldPlaced];
		if (!Zone.Hash) continue;

		FMW6XModel XModel;
		if (!MemoryReader->ReadMemory(StaticModel.XModel, XModel)) continue;

		FString XModelNameStr = TEXT("UnknownModel");
		if (XModel.NamePtr != 0)
		{
			MemoryReader->ReadString(XModel.NamePtr, XModelNameStr);
			XModelNameStr = FCoDAssetNameHelper::NoIllegalSigns(FPaths::GetBaseFilename(XModelNameStr));
		}
		else
		{
			XModelNameStr = FCoDDatabaseService::Get().GetPrintfAssetName(XModel.Hash, TEXT("xmodel"));
		}

		for (uint32 InstanceOffset = 0; InstanceOffset < StaticModelCollection.InstanceCount; ++InstanceOffset)
		{
			uint32 InstanceId = StaticModelCollection.FirstInstance + InstanceOffset;
			if (InstanceId >= SModels.InstanceCount)
			{
				UE_LOG(LogTemp, Warning, TEXT("InstanceId %u out of bounds (%u). Skipping."), InstanceId,
				       SModels.InstanceCount);
				continue;
			}

			FMW6GfxSModelInstanceData InstanceData;
			if (!MemoryReader->ReadMemory(SModels.InstanceData + InstanceId * sizeof(FMW6GfxSModelInstanceData),
			                              InstanceData))
				continue;

			// --- Create Instance Info ---
			FWraithMapStaticModelInstance& InstanceInfo = OutMapData.StaticModelInstances.AddDefaulted_GetRef();
			InstanceInfo.ModelAssetPtr = StaticModel.XModel;
			InstanceInfo.InstanceName = FString::Printf(TEXT("%s_Inst_%d"), *XModelNameStr, InstanceId);

			const float PosScale = 0.000244140625f; // 1/4096
			FVector Position(
				InstanceData.Translation[0] * PosScale,
				InstanceData.Translation[1] * PosScale,
				InstanceData.Translation[2] * PosScale
			);

			FQuat Rotation(
				FMath::Clamp(InstanceData.Orientation[0] * 0.000030518044f - 1.0f, -1.0f, 1.0f),
				FMath::Clamp(InstanceData.Orientation[1] * 0.000030518044f - 1.0f, -1.0f, 1.0f),
				FMath::Clamp(InstanceData.Orientation[2] * 0.000030518044f - 1.0f, -1.0f, 1.0f),
				FMath::Clamp(InstanceData.Orientation[3] * 0.000030518044f - 1.0f, -1.0f, 1.0f)
			);
			Rotation.Normalize();

			FFloat16 HalfFloatScale;
			HalfFloatScale.Encoded = InstanceData.HalfFloatScale;
			float Scale = HalfFloatScale;
			FVector Scale3D(Scale);

			InstanceInfo.Transform = FTransform(
				FQuat(Rotation.X, -Rotation.Y, Rotation.Z, -Rotation.W),
				FVector(Position.X, -Position.Y, Position.Z),
				Scale3D
			);
			InstanceInfo.Transform.NormalizeRotation();
		}
	}
	return true;
}

bool FModernWarfare6AssetHandler::TranslateModel(FWraithXModel& InModel, int32 LodIdx,
                                                 FCastModelInfo& OutModelInfo, const FCastRoot& InSceneRoot)
{
	OutModelInfo.Name = InModel.ModelName;

	if (!InModel.ModelLods.IsValidIndex(LodIdx))
	{
		UE_LOG(LogTemp, Error, TEXT("TranslateModel: Invalid LOD index %d for model %s"), LodIdx, *InModel.ModelName);
		return false;
	}
	FWraithXModelLod& LodRef = InModel.ModelLods[LodIdx];

	for (int32 SubmeshIdx = 0; SubmeshIdx < LodRef.Submeshes.Num(); ++SubmeshIdx)
	{
		if (!LodRef.Materials.IsValidIndex(SubmeshIdx))
		{
			UE_LOG(LogTemp, Warning, TEXT("TranslateModel: Material index %d out of bounds for LOD %d of model %s."),
			       SubmeshIdx, LodIdx, *InModel.ModelName);
			continue;
		}
		FWraithXMaterial& Material = LodRef.Materials[SubmeshIdx];
		FWraithXModelSubmesh& Submesh = LodRef.Submeshes[SubmeshIdx];

		if (const uint32* FoundIndex = InSceneRoot.MaterialMap.Find(Material.MaterialHash))
		{
			Submesh.MaterialIndex = *FoundIndex;
			Submesh.MaterialHash = Material.MaterialHash;
		}
		else
		{
			UE_LOG(LogTemp, Error,
			       TEXT(
				       "TranslateModel: Material hash %llu (Material: %s) not found in pre-processed SceneRoot.MaterialMap for LOD %d of model %s. This indicates an error in the pre-processing step."
			       ),
			       Material.MaterialHash, *Material.MaterialName, LodIdx, *InModel.ModelName);
			Submesh.MaterialIndex = INDEX_NONE;
			Submesh.MaterialHash = Material.MaterialHash;
		}
	}
	if (InModel.IsModelStreamed)
	{
		LoadXModel(InModel, LodRef, OutModelInfo);
	}
	else
	{
	}
	return true;
}

bool FModernWarfare6AssetHandler::TranslateAnim(const FWraithXAnim& InAnim, FCastAnimationInfo& OutAnimInfo)
{
	OutAnimInfo.Name = InAnim.AnimationName;
	OutAnimInfo.Framerate = InAnim.FrameRate;
	OutAnimInfo.Looping = InAnim.LoopingAnimation;

	OutAnimInfo.Type = ECastAnimationType::Relative;
	if (InAnim.DeltaTranslationPtr || InAnim.Delta2DRotationsPtr || InAnim.Delta3DRotationsPtr)
	{
		OutAnimInfo.Type = ECastAnimationType::Delta;
		OutAnimInfo.DeltaTagName = "tag_origin";
	}
	if (InAnim.AdditiveAnimation)
	{
		OutAnimInfo.Type = ECastAnimationType::Additive;
	}

	LoadXAnim(InAnim, OutAnimInfo);

	for (auto& NoteTrack : InAnim.Reader.Notetracks)
	{
		OutAnimInfo.AddNotificationKey(NoteTrack.Key, NoteTrack.Value);
	}
	if (InAnim.DeltaTranslationPtr)
	{
		DeltaTranslations64(OutAnimInfo, (InAnim.FrameCount > 255) ? 2 : 1, InAnim);
	}
	if (InAnim.Delta2DRotationsPtr)
	{
		Delta2DRotations64(OutAnimInfo, (InAnim.FrameCount > 255) ? 2 : 1, InAnim);
	}
	if (InAnim.Delta3DRotationsPtr)
	{
		Delta3DRotations64(OutAnimInfo, (InAnim.FrameCount > 255) ? 2 : 1, InAnim);
	}
	return false;
}

bool FModernWarfare6AssetHandler::LoadStreamedModelData(const FWraithXModel& InModel, FWraithXModelLod& InOutLod,
                                                        FCastModelInfo& OutModelInfo)
{
	return false;
}

bool FModernWarfare6AssetHandler::LoadStreamedAnimData(const FWraithXAnim& InAnim, FCastAnimationInfo& OutAnimInfo)
{
	return false;
}

void FModernWarfare6AssetHandler::LoadXModel(FWraithXModel& InModel, FWraithXModelLod& ModelLod,
                                             FCastModelInfo& OutModel)
{
	FMW6XModelSurfs MeshInfo;
	MemoryReader->ReadMemory<FMW6XModelSurfs>(ModelLod.LODStreamInfoPtr, MeshInfo);
	FMW6XSurfaceShared BufferInfo;
	MemoryReader->ReadMemory<FMW6XSurfaceShared>(MeshInfo.Shared, BufferInfo);

	TArray<uint8> MeshDataBuffer;
	if (BufferInfo.Data)
	{
		MemoryReader->ReadArray(BufferInfo.Data, MeshDataBuffer, BufferInfo.DataSize);
	}
	else
	{
		MeshDataBuffer = GameProcess->GetDecrypt()->ExtractXSubPackage(MeshInfo.XPakKey, BufferInfo.DataSize);
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

		Mesh.VertexUVs.SetNum(1);
		Mesh.VertexUVs[0].Reserve(Submesh.VertexCount);

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

			uint16 HalfUvU, HalfUvV;
			VertexUVReader << HalfUvU << HalfUvV;
			float HalfU = HalfFloatHelper::ToFloat(HalfUvU);
			float HalfV = HalfFloatHelper::ToFloat(HalfUvV);
			Mesh.VertexUVs[0].Emplace(HalfU, HalfV);
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
			FCoDMeshHelper::UnpackFaceIndices(MemoryReader, Faces, TableOffsetPtr, Submesh.PackedIndexTableCount,
			                                  PackedIndices, IndicesPtr, TriIdx, true);

			Mesh.Faces.Add(Faces[2]);
			Mesh.Faces.Add(Faces[1]);
			Mesh.Faces.Add(Faces[0]);
		}
	}
}

void FModernWarfare6AssetHandler::LoadXAnim(const FWraithXAnim& InAnim, FCastAnimationInfo& OutAnim)
{
	FMW6XAnim AnimHeader;
	MemoryReader->ReadMemory(InAnim.ReaderInformationPointer, AnimHeader);

	uint8* DataByte = nullptr;
	int16* DataShort = nullptr;
	int32* DataInt = nullptr;

	TArray<uint8*> RandomDataBytes;
	TArray<int16*> RandomDataShorts;

	OutAnim.Type = ECastAnimationType::Relative;

	TArray<uint32> AnimPackedInfo;
	MemoryReader->ReadArray(AnimHeader.DataInfo.PackedInfoPtr, AnimPackedInfo,
	                        AnimHeader.DataInfo.PackedInfoCount);
	TArray<uint8> IndicesBuffer;
	MemoryReader->ReadArray(AnimHeader.IndicesPtr, IndicesBuffer,
	                        AnimHeader.FrameCount >= 0x100 ? AnimHeader.IndexCount * 2 : AnimHeader.IndexCount);

	uint16* Indices = reinterpret_cast<uint16*>(IndicesBuffer.GetData());

	TArray<TArray<uint8>> AnimBuffers;
	for (uint32 CountIdx = 0; CountIdx < AnimHeader.DataInfo.OffsetCount; ++CountIdx)
	{
		FMW6XAnimStreamInfo StreamInfo;
		MemoryReader->ReadMemory<FMW6XAnimStreamInfo>(
			AnimHeader.DataInfo.StreamInfoPtr + CountIdx * sizeof(FMW6XAnimStreamInfo), StreamInfo);

		TArray<uint8> AnimBuffer = GameProcess->GetDecrypt()->ExtractXSubPackage(
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
		uint32 RandomDataAOffset;
		MemoryReader->ReadMemory<uint32>(AnimHeader.DataInfo.OffsetPtr2 + CountIdx * 4, RandomDataAOffset);
		uint32 RandomDataBOffset;
		MemoryReader->ReadMemory<uint32>(AnimHeader.DataInfo.OffsetPtr + CountIdx * 4, RandomDataBOffset);

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
			MW6XAnimCalculateBufferIndex(State, TableSize + 1, Idx);
			int16* RandomDataShort = RandomDataShorts[State.BufferIndex] + 2 * State.BufferOffset;
			float RZ = (float)RandomDataShort[0] * 0.000030518509f;
			float RW = (float)RandomDataShort[1] * 0.000030518509f;

			OutAnim.AddRotationKey(InAnim.Reader.BoneNames[CurrentBoneIndex], Frame, FVector4(0, 0, RZ, RW));
		}
		MW6XAnimIncrementBuffers(State, TableSize + 1, 2, RandomDataShorts);
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
			MW6XAnimCalculateBufferIndex(State, TableSize + 1, i);

			int16* RandomDataShort = RandomDataShorts[State.BufferIndex] + 4 * State.BufferOffset;
			float RX = (float)RandomDataShort[0] * 0.000030518509f;
			float RY = (float)RandomDataShort[1] * 0.000030518509f;
			float RZ = (float)RandomDataShort[2] * 0.000030518509f;
			float RW = (float)RandomDataShort[3] * 0.000030518509f;

			OutAnim.AddRotationKey(InAnim.Reader.BoneNames[CurrentBoneIndex], Frame, FVector4(RX, RY, RZ, RW));
		}
		MW6XAnimIncrementBuffers(State, TableSize + 1, 4, RandomDataShorts);
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
			MW6XAnimCalculateBufferIndex(State, TableSize + 1, i);

			uint8* RandomDataByte = RandomDataBytes[State.BufferIndex] + 3 * State.BufferOffset;
			float TranslationX = (FrameVecX * (float)RandomDataByte[0]) + MinsVecX;
			float TranslationY = (FrameVecY * (float)RandomDataByte[1]) + MinsVecY;
			float TranslationZ = (FrameVecZ * (float)RandomDataByte[2]) + MinsVecZ;

			OutAnim.AddTranslationKey(InAnim.Reader.BoneNames[BoneIndex], Frame,
			                          FVector(TranslationX, TranslationY, TranslationZ));
		}
		MW6XAnimIncrementBuffers(State, TableSize + 1, 3, RandomDataBytes);
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
			MW6XAnimCalculateBufferIndex(State, TableSize + 1, i);
			int16* RandomDataShort = RandomDataShorts[State.BufferIndex] + 3 * State.BufferOffset;

			float TranslationX = (FrameVecX * (float)(uint16)RandomDataShort[0]) + MinsVecX;
			float TranslationY = (FrameVecY * (float)(uint16)RandomDataShort[1]) + MinsVecY;
			float TranslationZ = (FrameVecZ * (float)(uint16)RandomDataShort[2]) + MinsVecZ;

			OutAnim.AddTranslationKey(InAnim.Reader.BoneNames[BoneIndex], Frame,
			                          FVector(TranslationX, TranslationY, TranslationZ));
		}
		MW6XAnimIncrementBuffers(State, TableSize + 1, 3, RandomDataShorts);
	}
	CurrentBoneIndex = 0;
	CurrentSize = InAnim.StaticTranslatedBoneCount;
	while (CurrentBoneIndex++ < CurrentSize)
	{
		FVector3f Vec = *(FVector3f*)DataInt;
		DataInt += 3;
		int16 BoneIndex = *DataShort++;

		OutAnim.AddTranslationKey(InAnim.Reader.BoneNames[BoneIndex], 0, FVector(Vec));
	}
}

void FModernWarfare6AssetHandler::MW6XAnimCalculateBufferIndex(FMW6XAnimBufferState& AnimState, const int32 TableSize,
                                                               const int32 KeyFrameIndex)
{
	if (TableSize < 4 || AnimState.OffsetCount == 1)
	{
		AnimState.BufferIndex = 0;
		AnimState.BufferOffset = KeyFrameIndex;
	}
	else
	{
		for (int32 i = 0; i < AnimState.OffsetCount; ++i)
		{
			if (((0x80000000 >> ((KeyFrameIndex + (i * TableSize) + AnimState.PackedPerFrameOffset) & 0x1F)) &
					AnimState.PackedPerFrameInfo[(KeyFrameIndex + (i * TableSize) + AnimState.PackedPerFrameOffset) >>
						5])
				!= 0)
			{
				AnimState.BufferIndex = i;
				AnimState.BufferOffset = MW6XAnimCalculateBufferOffset(
					AnimState, AnimState.PackedPerFrameOffset + TableSize * AnimState.BufferIndex, KeyFrameIndex);
				return;
			}
		}
	}
}

int32 FModernWarfare6AssetHandler::MW6XAnimCalculateBufferOffset(const FMW6XAnimBufferState& AnimState,
                                                                 const int32 Index, const int32 Count)
{
	if (Count == 0)
		return 0;

	int32 Mask = Index + Count - 1;
	int32 Start = Index >> 5;
	int32 End = Mask >> 5;
	int32 Result = 0;

	for (int32 i = Start; i <= End; i++)
	{
		if (AnimState.PackedPerFrameInfo[i] == 0)
			continue;

		uint32 maskA = 0xFFFFFFFF;
		uint32 maskB = 0xFFFFFFFF;

		if (i == Start)
			maskA = 0xFFFFFFFF >> (Index & 0x1F);
		if (i == End)
			maskB = 0xFFFFFFFF << (31 - (Mask & 0x1F));

		Result += FPlatformMath::CountBits(AnimState.PackedPerFrameInfo[i] & maskA & maskB);
	}

	return Result;
}

void FModernWarfare6AssetHandler::DeltaTranslations64(FCastAnimationInfo& OutAnim, uint32 FrameSize,
                                                      const FWraithXAnim& InAnim)
{
	uint64 DeltaTranslationPtr = InAnim.DeltaTranslationPtr;
	uint16 FrameCount;
	ReadAndAdvance(DeltaTranslationPtr, FrameCount);

	uint8 DataSize;
	// Advance 1 byte for frame size, 5 byte for padding
	ReadAndAdvance(DeltaTranslationPtr, DataSize, 6);

	FVector3f MinVec, SizeVec;
	ReadAndAdvance(DeltaTranslationPtr, MinVec);
	ReadAndAdvance(DeltaTranslationPtr, SizeVec);

	if (!FrameCount)
	{
		OutAnim.AddTranslationKey(TEXT("tag_origin"), 0, FVector(MinVec.X, MinVec.Y, MinVec.Z));
		return;
	}

	uint64 DeltaDataPtr;
	ReadAndAdvance(DeltaTranslationPtr, DeltaDataPtr);

	ProcessDeltaFramesAndAdd<FVector>(
		OutAnim, FrameCount, FrameSize, DeltaTranslationPtr, DeltaDataPtr, InAnim,
		[&](uint64& KeyStreamPtr, const FWraithXAnim&) -> FVector
		{
			float XCoord = 0.0f, YCoord = 0.0f, ZCoord = 0.0f;
			if (DataSize == 1)
			{
				uint8 XCoordData, YCoordData, ZCoordData;
				ReadAndAdvance(KeyStreamPtr, XCoordData);
				ReadAndAdvance(KeyStreamPtr, YCoordData);
				ReadAndAdvance(KeyStreamPtr, ZCoordData);
				XCoord = static_cast<float>(XCoordData);
				YCoord = static_cast<float>(YCoordData);
				ZCoord = static_cast<float>(ZCoordData);
			}
			else
			{
				uint16 XCoordData, YCoordData, ZCoordData;
				ReadAndAdvance(KeyStreamPtr, XCoordData);
				ReadAndAdvance(KeyStreamPtr, YCoordData);
				ReadAndAdvance(KeyStreamPtr, ZCoordData);
				XCoord = static_cast<float>(XCoordData);
				YCoord = static_cast<float>(YCoordData);
				ZCoord = static_cast<float>(ZCoordData);
			}

			FVector Translation;
			Translation.X = SizeVec.X * XCoord + MinVec.X;
			Translation.Y = SizeVec.Y * YCoord + MinVec.Y;
			Translation.Z = SizeVec.Z * ZCoord + MinVec.Z;
			return Translation;
		},
		[](FCastAnimationInfo& Anim, const FString& Name, uint32 Index, FVector&& Key)
		{
			Anim.AddTranslationKey(Name, Index, MoveTemp(Key));
		}
	);
}

void FModernWarfare6AssetHandler::Delta2DRotations64(FCastAnimationInfo& OutAnim, uint32 FrameSize,
                                                     const FWraithXAnim& InAnim)
{
	ProcessDeltaRotationsInternal<FQuat2Data, sizeof(FQuat2Data)>(
		OutAnim, FrameSize, InAnim, InAnim.Delta2DRotationsPtr, ProcessRaw2DRotation
	);
}

void FModernWarfare6AssetHandler::Delta3DRotations64(FCastAnimationInfo& OutAnim, uint32 FrameSize,
                                                     const FWraithXAnim& InAnim)
{
	ProcessDeltaRotationsInternal<FQuatData, sizeof(FQuatData)>(
		OutAnim, FrameSize, InAnim, InAnim.Delta3DRotationsPtr, ProcessRaw3DRotation
	);
}

FVector4 FModernWarfare6AssetHandler::ProcessRaw2DRotation(const FQuat2Data& RawRotData, const FWraithXAnim& InAnim)
{
	if (InAnim.RotationType == EAnimationKeyTypes::DivideBySize)
	{
		return FVector4{
			0.0f,
			0.0f,
			(static_cast<float>(RawRotData.RotationZ) / 32768.f),
			(static_cast<float>(RawRotData.RotationW) / 32768.f)
		};
	}
	if (InAnim.RotationType == EAnimationKeyTypes::HalfFloat)
	{
		return FVector4{
			0.0f,
			0.0f,
			HalfFloatHelper::ToFloat(RawRotData.RotationZ),
			HalfFloatHelper::ToFloat(RawRotData.RotationW)
		};
	}
	if (InAnim.RotationType == EAnimationKeyTypes::QuatPackingA)
	{
		return VectorPacking::QuatPacking2DA(*reinterpret_cast<const uint32*>(&RawRotData));
	}
	return FVector4::Zero();
}

FVector4 FModernWarfare6AssetHandler::ProcessRaw3DRotation(const FQuatData& RawRotData, const FWraithXAnim& InAnim)
{
	if (InAnim.RotationType == EAnimationKeyTypes::DivideBySize)
	{
		return FVector4{
			(static_cast<float>(RawRotData.RotationX) / 32768.f),
			(static_cast<float>(RawRotData.RotationY) / 32768.f),
			(static_cast<float>(RawRotData.RotationZ) / 32768.f),
			(static_cast<float>(RawRotData.RotationW) / 32768.f)
		};
	}
	if (InAnim.RotationType == EAnimationKeyTypes::HalfFloat)
	{
		return FVector4{
			HalfFloatHelper::ToFloat(RawRotData.RotationX),
			HalfFloatHelper::ToFloat(RawRotData.RotationY),
			HalfFloatHelper::ToFloat(RawRotData.RotationZ),
			HalfFloatHelper::ToFloat(RawRotData.RotationW)
		};
	}
	if (InAnim.RotationType == EAnimationKeyTypes::QuatPackingA)
	{
		return VectorPacking::QuatPackingA(*reinterpret_cast<const uint64*>(&RawRotData));
	}
	return FVector4::Zero();
}

uint32 FModernWarfare6AssetHandler::ReadFrameIndexData(uint64& Ptr, uint32 FrameSize)
{
	uint32 FrameIndex = 0;
	if (FrameSize == 1)
	{
		uint8 FrameIdxData;
		ReadAndAdvance(Ptr, FrameIdxData);
		FrameIndex = FrameIdxData;
	}
	else if (FrameSize == 2)
	{
		uint16 FrameIdxData;
		ReadAndAdvance(Ptr, FrameIdxData);
		FrameIndex = FrameIdxData;
	}
	return FrameIndex;
}
