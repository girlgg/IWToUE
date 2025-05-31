#pragma once

// 为什么不使用 #pragma pack(push, 1) 或者 alignas 强制对齐？因为经过计算后发现实际上和 x64 平台默认对齐模数一样均为8字节

struct FMW6GfxSurface
{
	uint32 PosOffset;											// 0
	uint32 BaseIndex;											// 4
	uint32 TableIndex;											// 8
	uint32 UgbSurfDataIndex;									// 12
	uint32 MaterialIndex;										// 16
	uint16 TriCount;											// 20
	uint16 PackedIndicesTableCount;								// 22
	uint16 VertexCount;											// 24
	uint16 Unk3;												// 26
	uint32 Unk4;												// 28
	uint32 Unk5;												// 32
	uint32 PackedIndicesOffset;									// 36
};

struct FMW6GfxWorldDrawOffset
{
	float X;													// 0
	float Y;													// 4
	float Z;													// 8
	float Scale;												// 12
};

struct FMW6GfxUgbSurfData
{
	FMW6GfxWorldDrawOffset WorldDrawOffset;						// 0
	uint32 TransientZoneIndex;									// 16
	uint32 Unk0;												// 20
	uint32 Unk1;												// 24
	uint32 LayerCount;											// 28
	uint32 VertexCount;											// 32
	uint32 Unk2;												// 36
	uint32 XyzOffset;											// 40
	uint32 TangentFrameOffset;									// 44
	uint32 LMapOffset;											// 48
	uint32 ColorOffset;											// 52
	uint32 TexCoordOffset;										// 56
	uint32 Unk3;												// 60
	uint32 AccumulatedUnk2;										// 64
	uint32 NormalTransformOffset[7];							// 68
	uint32 DisplacementOffset[8];								// 96
};

struct FMW6Material
{
	uint64 Hash;												// 0
	uint8 Pad_08[24 - 8];										// 8
	uint8 TextureCount;											// 24
	uint8 Pad_19;												// 25
	uint8 LayerCount;											// 26
	uint8 ImageCount;											// 27
	uint8 Pad_1C[32 - 28];										// 28
	uint64 TechsetPtr;											// 32
	uint64 TextureTable;										// 40
	uint64 ImageTable;											// 48
	uint8 Pad_38[120 - 56];										// 56 Padding to 0x78 (120 bytes)
};

struct FMW6MaterialTextureDef
{
	uint8 Index;
	uint8 ImageIdx;
};

struct FMW6GfxWorldSurfaces
{
	uint32 Count;												// 0
	uint8 Pad_04[4];											// 4
	uint32 UgbSurfDataCount;									// 8
	uint32 MaterialCount;										// 12
	uint8 Pad_10[112 - 16];										// 16
	uint64 Surfaces;											// 112
	uint8 Pad_78[152 - 120];									// 120
	uint64 Materials;											// 152
	uint8 Pad_A0[168 - 160];									// 160
	uint64 UgbSurfData;											// 168
	uint64 WorldDrawOffsets;									// 176
	uint8 Pad_B8[296 - 184];									// 184
	uint32 BtndSurfacesCount;									// 296
	uint8 Pad_12C[304 - 300];									// 300
	uint64 BtndSurfaces;										// 304
	// uint8 Pad_138[392 - 312];								// 312 Padding to 392 bytes, don't do this
};

struct FMW6GfxWorldStaticModels
{
	uint8 Pad_00[4];											// 0
	uint32 SModelCount;											// 4
	uint32 CollectionsCount;									// 8
	uint32 InstanceCount;										// 12
	uint8 Pad_10[80 - 16];										// 16
	uint64 Surfaces;											// 80
	uint64 SModels;												// 88
	uint8 Pad_60[104 - 96];										// 96
	uint64 Collections;											// 104
	uint8 Pad_70[168 - 112];									// 112
	uint64 InstanceData;										// 168
	uint8 Pad_B0[384 - 176];									// 176 Padding to 384 bytes
};

struct FMW6GfxStaticModel
{
	uint64 XModel;												// 0
	uint8 Flags;												// 8
	uint8 FirstMtlSkinIndex;									// 9
	uint16 FirstStaticModelSurfaceIndex;						// 10
};

struct FMW6GfxStaticModelCollection
{
	uint32 FirstInstance;										// 0
	uint32 InstanceCount;										// 4
	uint16 SModelIndex;											// 8
	uint16 TransientGfxWorldPlaced;								// 10
	uint16 ClutterIndex;										// 12
	uint8 Flags;												// 14
	uint8 Pad;													// 15
};

struct FMW6GfxSModelInstanceData
{
	int32 Translation[3];										// 0
	uint16 Orientation[4];										// 12
	uint16 HalfFloatScale;										// 20
};

struct FMW6GfxImage
{
	uint64 Hash;
	uint8 Unk00[16];
	uint32 BufferSize;
	uint32 Unk02;
	uint16 Width;
	uint16 Height;
	uint16 Depth;
	uint16 Levels;
	uint8 UnkByte0;
	uint8 UnkByte1;
	uint8 ImageFormat;
	uint8 UnkByte3;
	uint8 UnkByte4;
	uint8 UnkByte5;
	uint8 MipCount;
	uint8 UnkByte7;
	uint64 MipMaps;
	uint64 LoadedImagePtr;
};

struct FMW6GfxMip
{
	uint64 HashID;
	uint64 Pad;
	uint64 PackedInfo;
};

struct FMW6GfxMipArray
{
	TArray<FMW6GfxMip> MipMaps;
	int32 GetImageSize(const int32 Index) const
	{
		if (Index >= MipMaps.Num())
		{
			return 0;
		}
		if (!Index)
		{
			return (MipMaps[0].PackedInfo >> 30) & 0x1FFFFFFF;
		}
		return  ((MipMaps[Index].PackedInfo >> 30) & 0x1FFFFFFF) - ((MipMaps[Index - 1].PackedInfo >> 30) & 0x1FFFFFFF);
	}
};

// 128字节
struct FMW6BoneInfo
{
	uint8 NumBones;												// 0
	uint8 NumRootBones;											// 1
	uint8 Pad_2[6 - 2];											// 2
	uint8 CosmeticBoneCount;									// 6
	uint8 Pad_8[72 - 7];										// 7
	uint64 BoneParentsPtr;										// 72
	uint64 RotationsPtr;										// 80
	uint64 TranslationsPtr;										// 88
	uint64 Ptr4;												// 96
	uint64 BaseMatriciesPtr;									// 104
	uint64 BoneIDsPtr;											// 112
	uint64 Ptr7;												// 120
};

// 232字节
struct FMW6XModel
{
	uint64 Hash;												// 0
	uint64 NamePtr;												// 8
	uint16 NumSurfs;											// 16
	uint8 NumLods;												// 18
	uint8 MaxLods;												// 19
	uint8 Pad_20[40 - 20];										// 20
	float Scale;												// 40
	uint8 Pad_44[112 - 44];										// 44
	uint64 BoneInfoPtr;											// 112
	uint8 Pad_120[144 - 120];									// 120
	uint64 MaterialHandles;										// 144
	uint64 LodInfo;												// 152
	uint8 Pad_160[200 - 160];									// 160
	uint64 BoneIdsPtr;											// 200
	uint64 ParentListPtr;										// 208
	uint64 RotationsPtr;										// 216
	uint64 TranslationsPtr;										// 224
	uint64 PartClassificationPtr;								// 232
	uint64 BaseMaterialsPtr;									// 240
	uint64 UnKnownPtr;											// 248
	uint64 UnknownPtr2;											// 256
	uint64 MaterialHandlesPtr;									// 264
	uint64 ModelLods;											// 272
	uint8 Pad_280[400 - 280];									// 280
};

struct FMW6XModelLod
{
	uint64 MeshPtr;
	uint64 SurfsPtr;

	float LodDistance[3];

	uint16 NumSurfs;
	uint16 SurfacesIndex;

	uint8 Padding[40];
};

struct FMW6GfxWorld
{
	uint64 Hash;												// 0
	uint64 BaseName;											// 8
	uint8 Pad_10[192 - 16];										// 16
	FMW6GfxWorldSurfaces Surfaces;								// 192
	uint8 Pad_1F0[560 - 504];									// 504
	FMW6GfxWorldStaticModels SModels;							// 560
	uint8 Pad_2E0[5660 - 944];									// 944
	uint32 TransientZoneCount;									// 5660
	uint64 TransientZones[1536];								// 5664
};

struct FMW6XModelSurfs
{
	uint64 Hash;												// 0
	uint64 Surfs;												// 8
	uint64 XPakKey;												// 16
	uint8 Pad_24[32 - 24];										// 24
	uint64 Shared;												// 32
	uint16 NumSurfs;											// 40
	uint8 Padding2[80 - 42];									// 42
};

struct FMW6XSurfaceShared
{
	uint64 Data;
	uint32 DataSize;
	int32 Flags;
};

struct FMW6XSurface
{
	uint16 StatusFlag;										// 0
	uint16 FaceCountOld;										// 2
	uint16 PackedIndicesTableCount;								// 4
	uint8 Pad_06[24 - 6];										// 6
	uint32 VertCount;											// 24
	uint32 TriCount;											// 28
	uint8 Pad_08[36 - 32];										// 32
	uint16 WeightCounts[8];										// 36
	uint8 Pad_56[60 - 52];										// 56
	float OverrideScale;										// 60
	uint32 XyzOffset;											// 64
	uint32 TexCoordOffset;										// 68
	uint32 TangentFrameOffset;									// 72
	uint32 IndexDataOffset;										// 76
	uint32 PackedIndiciesTableOffset;							// 80
	uint32 PackedIndicesOffset;									// 84
	uint32 VertexColorOffset;									// 88
	uint32 ColorOffset;											// 92
	uint32 SecondUVOffset;										// 96
	uint8 Pad_100[108 - 100];									// 100
	uint32 WeightsOffset;										// 108
	uint8 Pad_112[120 - 112];									// 112
	uint64 Shared; // MeshBufferPointer							// 120
	uint8 Pad_124[176 - 128];									// 128
	float OffsetsX;												// 176
	float OffsetsY;												// 180
	float OffsetsZ;												// 184
	float Scale;												// 188
	float Min;													// 192
	float Max;													// 196
	uint8 Pad_200[224 - 200];									// 200
};

struct FMW6GfxWorldDrawVerts
{
	uint32 PosSize;												// 0
	uint32 IndexCount;											// 4
	uint32 TableCount;											// 8
	uint32 PackedIndicesSize;									// 12
	uint64 PosData;												// 16
	uint64 Indices;												// 24
	uint64 TableData;											// 32
	uint64 PackedIndices;										// 40
};

struct FMW6GfxWorldTransientZone
{
	uint64 Hash;												// 0
	uint8 Pad_08[20 - 8];										// 8
	uint32 TransientZoneIndex;									// 20
	FMW6GfxWorldDrawVerts DrawVerts;							// 24
	uint8 Pad_4C[376 - 72];										// 72 Padding to 376 bytes
};

struct FMW6XAnimDataInfo
{
	int32 DataByteOffset;
	int32 DataShortOffset;
	int32 DataIntOffset;
	int32 Padding4;
	uint64 OffsetPtr;
	uint64 OffsetPtr2;
	uint32 StreamIndex;
	uint32 OffsetCount;
	uint64 PackedInfoPtr;
	uint32 PackedInfoCount;
	uint32 PackedInfoCount2;
	uint64 Flags;
	uint64 StreamInfoPtr;
};

struct FMW6XAnimDeltaParts
{
	uint64 DeltaTranslationsPtr;
	uint64 Delta2DRotationsPtr;
	uint64 Delta3DRotationsPtr;
};

struct FMW6XAnim
{
	uint64 Hash;
	uint64 BoneIDsPtr;
	uint64 IndicesPtr;
	uint64 NotificationsPtr;
	uint64 DeltaPartsPtr;
	uint8 Padding[16];
	uint32 RandomDataShortCount;
	uint32 RandomDataByteCount;
	uint32 IndexCount;

	float Framerate;
	float Frequency;

	uint32 DataByteCount;
	uint16 DataShortCount;
	uint16 DataIntCount;
	uint16 RandomDataIntCount;
	uint16 FrameCount;

	uint8 Padding2[6];

	uint16 NoneRotatedBoneCount;
	uint16 TwoDRotatedBoneCount;
	uint16 NormalRotatedBoneCount;
	uint16 TwoDStaticRotatedBoneCount;
	uint16 NormalStaticRotatedBoneCount;
	uint16 NormalTranslatedBoneCount;
	uint16 PreciseTranslatedBoneCount;
	uint16 StaticTranslatedBoneCount;
	uint16 NoneTranslatedBoneCount;
	uint16 TotalBoneCount;

	uint8 NotetrackCount;
	uint8 AssetType;

	uint8 Padding3[20];

	FMW6XAnimDataInfo DataInfo;
};

struct FMW6SoundAsset
{
	uint64 Hash;
	uint64 Unk;
	uint64 StreamKeyEx;
	uint64 StreamKey;
	uint32 Size;
	uint32 Unk3;
	uint64 Unk4;
	uint64 Unk5;
	uint32 SeekTableSize;
	uint32 LoadedSize;
	uint32 FrameCount;
	uint32 FrameRate;
	uint32 Unk9;
	uint8 Unk10;
	uint8 Unk11;
	uint8 Unk12;
	uint8 ChannelCount;
	uint64 Unk13;
};

inline uint32 GMW6DXGIFormats[52]
{
	0,
	61,
	65,
	61,
	49,
	49,
	28,
	29,
	63,
	51,
	56,
	35,
	11,
	58,
	37,
	13,
	54,
	34,
	10,
	41,
	16,
	2,
	55,
	40,
	20,
	62,
	57,
	42,
	17,
	3,
	25,
	85,
	115,
	24,
	67,
	26,
	71,
	72,
	74,
	74,
	77,
	78,
	80,
	83,
	84,
	95,
	96,
	98,
	99,
	31,
	0,
	0
};

struct FMW6XAnimBufferState
{
	// The packed per frame information.
	TArray<uint32> PackedPerFrameInfo{};
	// The current buffer index requested from the packed buffer.
	int32 BufferIndex = 0;
	// The offset within the buffer from the packed buffer.
	int32 BufferOffset = 0;
	// The number of buffers.
	int32 OffsetCount = 0;
	// The current offset within the packed buffer.
	int32 PackedPerFrameOffset = 0;
};

struct FMW6XAnimNoteTrack
{
	uint32 Name;
	float Time;
	uint8 Padding[24];
};

struct FMW6XAnimStreamInfo
{
	uint64 StreamKey;
	uint32 Size;
	uint32 Padding;
};