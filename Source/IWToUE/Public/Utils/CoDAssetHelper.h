#pragma once

class IMemoryReader;
class FGameProcess;

namespace FCoDAssetHelper
{
	static UTexture2D* CreateTextureFromDDSData(const TArray<uint8>& ImageDataArray, int32 Width, int32 Height,
	                                            uint8 ImageFormat, const FString& TextureName);
	static bool SaveObjectToPackage(UObject* ObjectToSave, const FString& PackagePath);
}

namespace FCoDMeshHelper
{
	uint8 FindFaceIndex(TSharedPtr<IMemoryReader>& MemoryReader, uint64 PackedIndices, uint32 Index, uint8 Bits,
	                    bool IsLocal = false);

	bool UnpackFaceIndices(TSharedPtr<IMemoryReader>& MemoryReader, TArray<uint16>& InFacesArr, uint64 Tables,
	                       uint64 TableCount, uint64 PackedIndices,
	                       uint64 Indices, uint64 FaceIndex, const bool IsLocal = false);
	void UnpackCoDQTangent(const uint32 Packed, FVector3f& Tangent, FVector3f& Normal);
};

namespace VectorPacking
{
	FVector4 QuatPackingA(const uint64 PackedData);
	FVector4 QuatPacking2DA(const uint32 PackedData);
};
