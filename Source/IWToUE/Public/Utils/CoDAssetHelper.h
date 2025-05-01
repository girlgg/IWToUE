#pragma once
#include "AssetRegistry/AssetRegistryModule.h"

class IGameAssetHandler;
class IMemoryReader;
class FGameProcess;

namespace HalfFloatHelper
{
	union FFloatBits
	{
		float F;
		int32 SI;
		uint32 UI;
	};

	uint16 ToHalfFloat(float Value);
	float ToFloat(uint16 HalfFloat);

	static constexpr int32 Shift = 13;
	static constexpr int32 ShiftSign = 16;

	static constexpr int32 InfN = 0x7F800000;
	static constexpr int32 MaxN = 0x477FE000;
	static constexpr int32 MinN = 0x38800000;
	static constexpr int32 SignN = 0x80000000;

	static constexpr int32 InfC = InfN >> Shift;
	static constexpr int32 NanN = (InfC + 1) << Shift;
	static constexpr int32 MaxC = MaxN >> Shift;
	static constexpr int32 MinC = MinN >> Shift;
	static constexpr int32 SignC = SignN >> ShiftSign;

	static constexpr int32 MulN = 0x52000000;
	static constexpr int32 MulC = 0x33800000;

	static constexpr int32 SubC = 0x003FF;
	static constexpr int32 NorC = 0x00400;

	static constexpr int32 MaxD = InfC - MaxC - 1;
	static constexpr int32 MinD = MinC - SubC - 1;
};

/**
 * @brief Helper class for asset operations.
 */
namespace FCoDAssetHelper
{
	struct FPreparedTextureData
	{
		FString TextureName;
		FString PackagePath;
		size_t Width = 0;
		size_t Height = 0;
		EPixelFormat FinalPixelFormat = PF_Unknown;
		ETextureSourceFormat SourceDataFormat = TSF_Invalid;
		TextureCompressionSettings CompressionSettings = TC_Default;
		bool bSRGB = false;
		TArray<uint8> Mip0Data;

		bool IsValid() const
		{
			return Width > 0 && Height > 0 && !Mip0Data.IsEmpty() &&
				   FinalPixelFormat != PF_Unknown && SourceDataFormat != TSF_Invalid;
		}
	};

	/**
	 * @brief Creates/configures the UTexture2D asset. MUST run on the Game Thread.
	 * @param PreparedData The data prepared by the calling thread.
	 * @return Pointer to the created UTexture2D, or nullptr on failure.
	 */
	UTexture2D* CreateTextureOnGameThread(FPreparedTextureData PreparedData);
	/**
	 * @brief Creates a UTexture2D asset from raw DDS image data. Ensures UObject operations run on the Game Thread.
	 * Handles SRGB correctly by letting DirectXTex convert to linear, then setting the SRGB flag in Unreal.
	 *
	 * @param DDSDataArray The raw byte array containing the DDS file data.
	 * @param TextureName The desired name for the new Texture2D asset.
	 * @param PackagePath The desired package path (e.g., "/Game/Textures").
	 * @return A pointer to the newly created UTexture2D, or nullptr if creation failed.
	 */
	UTexture2D* CreateTextureFromDDSData(const TArray<uint8>& DDSDataArray, const FString& TextureName,
	                                     const FString& PackagePath);
	/**
	 * Creates a USoundWave asset from raw WAV audio data.
	 *
	 * @param Outer The outer object (package) for the new SoundWave. Defaults to the transient package.
	 * @param AssetName The desired name for the new SoundWave asset.
	 * @param AudioData The TArray containing the raw WAV file data.
	 * @param ObjectFlags The object flags to apply to the new SoundWave. Defaults to RF_Public | RF_Standalone.
	 * @return A pointer to the newly created USoundWave, or nullptr if creation failed.
	 */
	USoundWave* CreateSoundWaveFromData(
		UObject* Outer,
		const FString& AssetName,
		const TArray<uint8>& AudioData,
		EObjectFlags ObjectFlags = RF_Public | RF_Standalone);

	TArray<uint8> BuildDDSHeader(uint32 Width, uint32 Height, uint32 Depth, uint32 MipLevels, uint8 Format,
	                             bool bIsCubeMap);
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


namespace FCoDAssetNameHelper
{
	FString NoIllegalSigns(const FString& InString);

	template <typename T>
	bool FindAssetByName(const FString& BaseAssetName, T*& OutAsset, int32 MaxAttempts = 100)
	{
		if (BaseAssetName.IsEmpty())
		{
			return false;
		}
		FAssetRegistryModule& AssetRegistryModule =
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		TArray<FAssetData> AssetDataArray;

		FString FinalAssetName = BaseAssetName;
		int32 NameIndex = 1;

		while (NameIndex <= MaxAttempts)
		{
			FString AssetName = FString::Printf(TEXT("/Game/%s"), *FinalAssetName);
			AssetRegistryModule.Get().GetAssetsByPackageName(*AssetName, AssetDataArray);
			for (const FAssetData& AssetData : AssetDataArray)
			{
				if (AssetData.GetClass() == T::StaticClass())
				{
					OutAsset = Cast<T>(AssetData.GetAsset());
					if (OutAsset)
					{
						return true;
					}
				}
			}
			FinalAssetName = FString::Printf(TEXT("%s_%d"), *BaseAssetName, NameIndex++);
			AssetDataArray.Empty();
		}
		return false;
	}
};
