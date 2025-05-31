#pragma once

struct FMW5XModelLod
{
    uint64 MeshPtr;
    uint64 SurfsPtr;

    float LodDistance[3];

    uint16 NumSurfs;
    uint16 SurfacesIndex;

    uint8 Padding[40];
};

struct FMW5XModel
{
    uint64 Hash;
    uint64 NamePtr;
    uint16 NumSurfaces;
    uint8 NumLods;
    uint8 MaxLods;

    uint8 Padding[12];

    uint8 NumBones;
    uint8 NumRootBones;
    uint16 UnkBoneCount;

    uint8 Padding3[184 - 36];
    
    uint64 BoneIDsPtr;
    uint64 ParentListPtr;
    uint64 RotationsPtr;
    uint64 TranslationsPtr;
    uint64 PartClassificationPtr;
    uint64 BaseMatriciesPtr;
    uint64 UnknownPtr;
    uint64 UnknownPtr2;
    uint64 MaterialHandlesPtr;
    uint64 ModelLods;

    uint8 Padding4[112];
};

struct FMW5XSurface
{
    uint16 StatusFlag;	    									// 0
    uint16 VertexCount;											// 2
    uint16 FacesCount;											// 4
    uint8 Pad_08[12 - 6];										// 6
    uint8 VertListCount;                                        // 12
    uint8 Pad_14[32 - 13];                                      // 13
    uint16 WeightCounts[8];										// 32
    uint8 Pad_48[56 - 48];										// 48
    float NewScale;                                             // 56
    uint32 VertexOffset;                                        // 60
    uint32 VertexUVsOffset;                                     // 64
    uint32 VertexTangentOffset;                                 // 68
    uint32 FacesOffset;                                         // 72
    uint32 Offset4;
    uint32 Offset5;
    uint32 VertexColorOffset;                                   // 84
    uint32 Offset7;
    uint32 Offset8;
    uint32 Offset9;
    uint32 WeightsOffset;                                       // 100
    uint32 Offset11;
    uint32 Offset12;                                            // 60
    uint64 MeshBufferPointer;                                   // 112
    uint64 RigidWeightsPtr;
    uint8 Padding4[40];
    float XOffset;
    float YOffset;
    float ZOffset;
    float Scale;
    float Min;
    float Max;
    uint8 Padding9[16];
};

struct FMW5Material
{
    uint64 Hash;												// 0
    uint8 Pad_08[24 - 8];										// 8
    uint8 ImageCount;											// 24
    uint8 Pad_25[40 - 25];										// 28
    uint64 TechsetPtr;											// 40
    uint64 ImageTable;											// 48
    uint8 Pad_38[88 - 56];										// 56
};

struct FMW5GfxWorld
{
    uint64 Hash;												// 0
    uint64 BaseName;											// 8
};

struct FMW5XMaterialImage
{
    uint32 Type;
    uint8 Padding[4];
    uint64 ImagePtr;
};

inline uint32 GMW5DXGIFormats[52]
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

struct FMW5GfxImage
{
    uint64 Hash = 0;
    uint8 Unk00[16] = {};
    uint32 BufferSize = 0;
    uint32 Unk02 = 0;
    uint16 Width = 0;
    uint16 Height = 0;
    uint16 Depth = 0;
    uint16 Levels = 0;
    uint8 UnkByte0 = 0;
    uint8 UnkByte1 = 0;
    uint8 ImageFormat = 0;
    uint8 UnkByte3 = 0;
    uint8 UnkByte4 = 0;
    uint8 UnkByte5 = 0;
    uint8 MipCount = 0;
    uint8 UnkByte7 = 0;
    uint64 MipMaps = 0;
    uint64 PrimedMipPtr = 0;
    uint64 LoadedImagePtr = 0;
};

struct FMW5GfxMip
{
    uint64 HashID;
    uint64 Pad;
    uint64 PackedInfo;
};

struct FMW5GfxMipArray
{
    TArray<FMW5GfxMip> MipMaps;
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


struct FMW5XAnimDataInfo
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

struct FMW5XAnim
{
    uint64 Hash;
    uint64 NamePtr;
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

    FMW5XAnimDataInfo DataInfo;
};

struct FMW5XMaterial
{
    uint64 Hash;
    uint8 Padding[16];
    uint8 ImageCount;
    uint8 Padding2[15];
    uint64 TechsetPtr;
    uint64 ImageTablePtr;
    uint8 Padding3[32];
};

struct FMW5SoundAsset
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

struct FMW5XAnimStreamInfo
{
    uint64 StreamKey;
    uint32 Size;
    uint32 Padding;
};

struct FMW5XAnimNoteTrack
{
    uint32 Name;
    float Time;
    uint8 Padding[24];
};

struct FMW5XAnimDeltaParts
{
    uint64 DeltaTranslationsPtr;
    uint64 Delta2DRotationsPtr;
    uint64 Delta3DRotationsPtr;
};

struct FMW5XModelMesh
{
    uint64 Hash;
    uint64 SurfsPtr;
    uint64 LODStreamKey;
    uint8 Padding[0x8];

    uint64 MeshBufferPointer;
    uint16 NumSurfs;
    uint8 Padding2[38];

};

struct FMW5XModelMeshBufferInfo
{
    uint64_t BufferPtr;
    uint32_t BufferSize;
    uint32_t Streamed;
};