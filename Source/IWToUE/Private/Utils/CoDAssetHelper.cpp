#include "Utils/CoDAssetHelper.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "SeLogChannels.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "CastManager/CastModel.h"
#include "Interface/IMemoryReader.h"
#include "UObject/SavePackage.h"
#include "WraithX/CoDAssetType.h"
#include "WraithX/GameProcess.h"

#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "DirectXTex.h"
#include "DDS.h"
#include "Windows/HideWindowsPlatformTypes.h"

namespace FCoDAssetHelper
{
	bool GetUnrealFormat(DXGI_FORMAT DxgiFormat, EPixelFormat& OutPixelFormat, ETextureSourceFormat& OutSourceFormat,
	                     bool& bOutSRGB)
	{
		bOutSRGB = DirectX::IsSRGB(DxgiFormat);
		OutPixelFormat = PF_Unknown;
		OutSourceFormat = TSF_Invalid;

		switch (DxgiFormat)
		{
		// --- 未压缩格式 ---
		case DXGI_FORMAT_R10G10B10A2_UNORM:
			OutSourceFormat = TSF_BGRA8;
			OutPixelFormat = PF_A2B10G10R10;
			break;
		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_UNORM:
			OutSourceFormat = TSF_BGRA8;
			OutPixelFormat = PF_B8G8R8A8;
			break;
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			OutSourceFormat = TSF_BGRA8;
			OutPixelFormat = PF_B8G8R8A8;
			bOutSRGB = true;
			break;
		case DXGI_FORMAT_R8_UNORM:
		case DXGI_FORMAT_A8_UNORM:
			OutSourceFormat = TSF_G8;
			OutPixelFormat = PF_G8;
			break;
		case DXGI_FORMAT_R16G16B16A16_FLOAT:
			OutSourceFormat = TSF_RGBA16F;
			OutPixelFormat = PF_FloatRGBA;
			break;
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
			OutSourceFormat = TSF_RGBA16F;
			OutPixelFormat = PF_FloatRGBA;
			break;

		// --- Compressed Formats ---
		case DXGI_FORMAT_BC1_UNORM:
		case DXGI_FORMAT_BC1_UNORM_SRGB:
		case DXGI_FORMAT_BC2_UNORM:
		case DXGI_FORMAT_BC2_UNORM_SRGB:
		case DXGI_FORMAT_BC3_UNORM:
		case DXGI_FORMAT_BC3_UNORM_SRGB:
		case DXGI_FORMAT_BC7_UNORM:
		case DXGI_FORMAT_BC7_UNORM_SRGB:
			OutSourceFormat = TSF_BGRA8;
			OutPixelFormat = PF_B8G8R8A8;
			bOutSRGB = DirectX::IsSRGB(DxgiFormat);
			break;
		case DXGI_FORMAT_BC4_UNORM:
		case DXGI_FORMAT_BC4_SNORM:
			OutSourceFormat = TSF_G8;
			OutPixelFormat = PF_G8;
			break;
		case DXGI_FORMAT_BC5_UNORM:
		case DXGI_FORMAT_BC5_SNORM:
			OutSourceFormat = TSF_BGRA8;
			OutPixelFormat = PF_B8G8R8A8;
			break;
		case DXGI_FORMAT_BC6H_UF16:
		case DXGI_FORMAT_BC6H_SF16:
			OutSourceFormat = TSF_RGBA16F;
			OutPixelFormat = PF_FloatRGBA;
			break;

		default:
			UE_LOG(LogTemp, Warning, TEXT("Unsupported DXGI_FORMAT: %d"), static_cast<int>(DxgiFormat));
			return false;
		}

		return true;
	}

	TextureCompressionSettings GetCompressionSettingsFromDXGI(DXGI_FORMAT DxgiFormat)
	{
		switch (DxgiFormat)
		{
		case DXGI_FORMAT_BC1_UNORM:
		case DXGI_FORMAT_BC1_UNORM_SRGB:
		case DXGI_FORMAT_BC2_UNORM:
		case DXGI_FORMAT_BC2_UNORM_SRGB:
		case DXGI_FORMAT_BC3_UNORM:
		case DXGI_FORMAT_BC3_UNORM_SRGB:
		case DXGI_FORMAT_BC7_UNORM:
		case DXGI_FORMAT_BC7_UNORM_SRGB:
			return TC_Default;

		case DXGI_FORMAT_BC4_UNORM:
			return TC_Grayscale;
		case DXGI_FORMAT_BC4_SNORM:
			return TC_Displacementmap;
		case DXGI_FORMAT_BC5_UNORM:
		case DXGI_FORMAT_BC5_SNORM:
			return TC_Normalmap;

		case DXGI_FORMAT_BC6H_UF16:
		case DXGI_FORMAT_BC6H_SF16:
			return TC_HDR;

		case DXGI_FORMAT_R8_UNORM:
		case DXGI_FORMAT_A8_UNORM:
			return TC_Alpha;
		case DXGI_FORMAT_R8_SNORM:
			return TC_Displacementmap;

		case DXGI_FORMAT_R10G10B10A2_UNORM:
		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		case DXGI_FORMAT_R16G16B16A16_FLOAT:
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
		default:
			return TC_Default;
		}
	}
}

UTexture2D* FCoDAssetHelper::CreateTextureFromDDSData(const TArray<uint8>& DDSDataArray, const FString& TextureName,
                                                      const FString& PackagePath)
{
	if (DDSDataArray.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Input image data array is empty for %s"), *TextureName);
		return nullptr;
	}
	DirectX::ScratchImage ScratchImage;
	DirectX::TexMetadata Metadata;
	HRESULT hr = DirectX::LoadFromDDSMemory(DDSDataArray.GetData(), DDSDataArray.Num(),
	                                        DirectX::DDS_FLAGS::DDS_FLAGS_NONE,
	                                        &Metadata, ScratchImage);
	if (FAILED(hr))
	{
		hr = DirectX::LoadFromDDSMemory(DDSDataArray.GetData(), DDSDataArray.Num(),
		                                DirectX::DDS_FLAGS::DDS_FLAGS_LEGACY_DWORD,
		                                &Metadata, ScratchImage);
		if (FAILED(hr))
		{
			UE_LOG(LogTemp, Error,
			       TEXT("CreateTextureFromDDSData: Failed to load DDS data for %s using DirectXTex. HRESULT: 0x%X"),
			       *TextureName, hr);
			return nullptr;
		}
		UE_LOG(LogTemp, Warning, TEXT("CreateTextureFromDDSData: Loaded DDS %s using DDS_FLAGS_LEGACY_DWORD fallback."),
		       *TextureName);
	}
	if (Metadata.width == 0 || Metadata.height == 0 || Metadata.mipLevels == 0)
	{
		UE_LOG(LogTemp, Error,
		       TEXT(
			       "CreateTextureFromDDSData: DDS metadata loaded with invalid dimensions or mip count for %s (W:%zu H:%zu Mips:%zu)"
		       ),
		       *TextureName, Metadata.width, Metadata.height, Metadata.mipLevels);
		return nullptr;
	}

	EPixelFormat TargetPixelFormat = PF_Unknown;
	ETextureSourceFormat UnrealSourceFormat = TSF_Invalid;
	bool bSRGB = false;

	if (!GetUnrealFormat(Metadata.format, TargetPixelFormat, UnrealSourceFormat, bSRGB))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to map DXGI format %d to Unreal format for texture %s"),
		       static_cast<int>(Metadata.format), *TextureName);
		return nullptr;
	}

	DXGI_FORMAT TargetDxgiFormat = DXGI_FORMAT_UNKNOWN;

	switch (UnrealSourceFormat)
	{
	case TSF_BGRA8: TargetDxgiFormat = bSRGB ? DXGI_FORMAT_B8G8R8A8_UNORM_SRGB : DXGI_FORMAT_B8G8R8A8_UNORM;
		break;
	case TSF_RGBA16F: TargetDxgiFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
		break;
	case TSF_G8: TargetDxgiFormat = DXGI_FORMAT_R8_UNORM;
		break;
	case TSF_RGBA32F: TargetDxgiFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
		break;
	case TSF_R16F: TargetDxgiFormat = DXGI_FORMAT_R16_FLOAT;
		break;
	default:
		UE_LOG(LogTemp, Error,
		       TEXT("CreateTextureFromDDSData: Cannot determine target DXGI format for Unreal Source Format %d (%s)"),
		       (int)UnrealSourceFormat, *TextureName);
		return nullptr;
	}

	DirectX::ScratchImage ProcessedImageData;
	const DirectX::Image* SourceImagesPtr = ScratchImage.GetImages();
	const DirectX::TexMetadata* SourceMetadataPtr = &Metadata;
	size_t SourceImageCount = ScratchImage.GetImageCount();
	bool bCleanupProcessed = false;

	if (DirectX::IsCompressed(Metadata.format))
	{
		UE_LOG(LogTemp, Verbose, TEXT("CreateTextureFromDDSData: Decompressing %s from %d to %d"), *TextureName,
		       Metadata.format, TargetDxgiFormat);
		hr = DirectX::Decompress(SourceImagesPtr, SourceImageCount, Metadata, TargetDxgiFormat, ProcessedImageData);
		if (FAILED(hr))
		{
			UE_LOG(LogTemp, Error,
			       TEXT(
				       "CreateTextureFromDDSData: Failed to decompress DDS image %s from format %d to %d. HRESULT: 0x%X"
			       ), *TextureName, Metadata.format, TargetDxgiFormat, hr);
			return nullptr;
		}
		SourceImagesPtr = ProcessedImageData.GetImages();
		SourceMetadataPtr = &ProcessedImageData.GetMetadata();
		SourceImageCount = ProcessedImageData.GetImageCount();
		bCleanupProcessed = true;
	}
	else if (Metadata.format != TargetDxgiFormat)
	{
		UE_LOG(LogTemp, Verbose, TEXT("CreateTextureFromDDSData: Converting %s from %d to %d"), *TextureName,
		       Metadata.format, TargetDxgiFormat);
		hr = DirectX::Convert(SourceImagesPtr, SourceImageCount, Metadata, TargetDxgiFormat,
		                      DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, ProcessedImageData);
		if (FAILED(hr))
		{
			UE_LOG(LogTemp, Error,
			       TEXT("CreateTextureFromDDSData: Failed to convert DDS image %s from format %d to %d. HRESULT: 0x%X"),
			       *TextureName, Metadata.format, TargetDxgiFormat, hr);
			return nullptr;
		}
		SourceImagesPtr = ProcessedImageData.GetImages();
		SourceMetadataPtr = &ProcessedImageData.GetMetadata();
		SourceImageCount = ProcessedImageData.GetImageCount();
		bCleanupProcessed = true;
	}
	if (!SourceImagesPtr || SourceImageCount == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateTextureFromDDSData: No valid image data available after processing for %s"),
		       *TextureName);
		if (bCleanupProcessed) ProcessedImageData.Release();
		return nullptr;
	}

	FString PackageName = FPaths::Combine(PackagePath, TextureName);
	UPackage* Package = CreatePackage(*PackageName);
	Package->FullyLoad();
	Package->SetDirtyFlag(true);

	UTexture2D* NewTexture = NewObject<UTexture2D>(Package, FName(*TextureName),
	                                               RF_Public | RF_Standalone | RF_MarkAsRootSet);
	if (!NewTexture)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateTextureFromDDSData: Failed to create UTexture2D object for %s"),
		       *TextureName);
		if (bCleanupProcessed) ProcessedImageData.Release();
		return nullptr;
	}

	NewTexture->SetPlatformData(nullptr);
	NewTexture->SRGB = bSRGB;
	NewTexture->CompressionSettings = GetCompressionSettingsFromDXGI(Metadata.format);
	NewTexture->Filter = TF_Default;
	NewTexture->AddressX = TA_Clamp;
	NewTexture->AddressY = TA_Clamp;
	NewTexture->LODGroup = TEXTUREGROUP_World;

	if (NewTexture->CompressionSettings == TC_Normalmap)
	{
		NewTexture->LODGroup = TEXTUREGROUP_WorldNormalMap;
	}
	else if (NewTexture->CompressionSettings == TC_HDR || NewTexture->CompressionSettings == TC_HDR_Compressed)
	{
		NewTexture->LODGroup = TEXTUREGROUP_World;
	}

	// Initialize Source Data
	const DirectX::TexMetadata& FinalMetadata = *SourceMetadataPtr;
	NewTexture->Source.Init(
		FinalMetadata.width,
		FinalMetadata.height,
		FinalMetadata.depth > 1 ? FinalMetadata.depth : (FinalMetadata.arraySize > 1 ? FinalMetadata.arraySize : 1),
		FinalMetadata.mipLevels,
		UnrealSourceFormat
	);

	uint32 DestSliceIndex = 0;
	for (size_t MipIndex = 0; MipIndex < FinalMetadata.mipLevels; ++MipIndex)
	{
		const size_t ImageIndex = FinalMetadata.ComputeIndex(MipIndex, DestSliceIndex, 0);
		if (ImageIndex >= SourceImageCount)
		{
			UE_LOG(LogTemp, Warning,
			       TEXT("CreateTextureFromDDSData: Mip index %zu out of bounds for image count %zu in %s"), MipIndex,
			       SourceImageCount, *TextureName);
			continue;
		}

		const DirectX::Image& MipImage = SourceImagesPtr[ImageIndex];
		uint8* DestMipData = NewTexture->Source.LockMip(MipIndex);
		if (!DestMipData)
		{
			UE_LOG(LogTemp, Error, TEXT("CreateTextureFromDDSData: Failed to lock Mip %zu for texture %s"), MipIndex,
			       *TextureName);
			NewTexture->Source.UnlockMip(0);
			NewTexture->MarkAsGarbage();
			if (bCleanupProcessed) ProcessedImageData.Release();
			return nullptr;
		}
		SIZE_T ExpectedMipDataSize = NewTexture->Source.CalcMipSize(MipIndex);
		SIZE_T SourceMipDataSize = MipImage.slicePitch;
		if (MipImage.height > 1 && MipImage.rowPitch * MipImage.height != MipImage.slicePitch)
		{
			SIZE_T CalculatedSize = MipImage.rowPitch * MipImage.height;
			UE_LOG(LogTemp, Warning,
			       TEXT(
				       "CreateTextureFromDDSData: Mip %zu slicePitch (%zu) differs from rowPitch*height (%zu) for %s. Using calculated size."
			       ),
			       MipIndex, MipImage.slicePitch, CalculatedSize, *TextureName);
			SourceMipDataSize = CalculatedSize;
		}
		if (SourceMipDataSize == 0 || MipImage.pixels == nullptr)
		{
			UE_LOG(LogTemp, Warning,
			       TEXT("CreateTextureFromDDSData: Mip %zu has zero size or null data in source image for %s"),
			       MipIndex, *TextureName);
			NewTexture->Source.UnlockMip(MipIndex);
			continue;
		}
		if (ExpectedMipDataSize != SourceMipDataSize)
		{
			UE_LOG(LogTemp, Warning,
			       TEXT(
				       "CreateTextureFromDDSData: Mip %zu data size mismatch for %s. Expected: %llu, Got: %llu. Clamping copy size."
			       ),
			       MipIndex, *TextureName, ExpectedMipDataSize, SourceMipDataSize);
			SIZE_T CopySize = FMath::Min(ExpectedMipDataSize, SourceMipDataSize);
			FMemory::Memcpy(DestMipData, MipImage.pixels, CopySize);
		}
		else
		{
			FMemory::Memcpy(DestMipData, MipImage.pixels, SourceMipDataSize);
		}
		NewTexture->Source.UnlockMip(MipIndex);
	}


	if (bCleanupProcessed)
	{
		ProcessedImageData.Release();
	}
	ScratchImage.Release();

	NewTexture->UpdateResource();
	Package->MarkPackageDirty();

	FAssetRegistryModule::AssetCreated(NewTexture);

	// Save the package
	// FSavePackageArgs SaveArgs;
	// SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	// SaveArgs.SaveFlags = SAVE_Async; // Or SAVE_None
	// FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	// UPackage::SavePackage(Package, NewTexture, *PackageFileName, SaveArgs);

	NewTexture->ClearFlags(RF_MarkAsRootSet);

	UE_LOG(LogTemp, Log, TEXT("CreateTextureFromDDSData: Successfully created texture %s"), *TextureName);
	return NewTexture;
}

USoundWave* FCoDAssetHelper::CreateSoundWaveFromData(UObject* Outer, const FString& AssetName,
                                                     const TArray<uint8>& AudioData, EObjectFlags ObjectFlags)
{
	if (AudioData.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot create SoundWave '%s': Input audio data is empty."), *AssetName);
		return nullptr;
	}
	if (!Outer)
	{
		Outer = GetTransientPackage();
	}
	USoundWave* SoundWave = NewObject<USoundWave>(Outer, FName(*AssetName), ObjectFlags | RF_Transactional);
	if (!SoundWave)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create USoundWave object named '%s' in Outer '%s'."), *AssetName,
		       *Outer->GetPathName());
		return nullptr;
	}

	FWaveModInfo WaveInfo;
	FString ErrorMessage;
	const uint8* WaveDataPtr = AudioData.GetData();
	const int32 WaveDataSize = AudioData.Num();

	if (!WaveInfo.ReadWaveInfo(WaveDataPtr, WaveDataSize, &ErrorMessage))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to parse WAV header for sound '%s'. Error: %s"), *AssetName,
		       *ErrorMessage);
		WaveInfo.ReportImportFailure();
		return nullptr;
	}
	if (!WaveInfo.IsFormatSupported())
	{
		const uint16 FormatTag = WaveInfo.pFormatTag ? *WaveInfo.pFormatTag : 0;
		const uint16 BitsPerSample = WaveInfo.pBitsPerSample ? *WaveInfo.pBitsPerSample : 0;
		UE_LOG(LogTemp, Error,
		       TEXT(
			       "Unsupported WAV format for sound '%s'. FormatTag: %d, BitsPerSample: %d, Channels: %d, SampleRate: %d"
		       ),
		       *AssetName, FormatTag, BitsPerSample, *WaveInfo.pChannels, *WaveInfo.pSamplesPerSec);
		return nullptr;
	}

	SoundWave->NumChannels = *WaveInfo.pChannels;
	SoundWave->SetSampleRate(*WaveInfo.pSamplesPerSec);
	SoundWave->RawPCMDataSize = WaveInfo.SampleDataSize;
	SoundWave->SetSoundAssetCompressionType(ESoundAssetCompressionType::PCM);

	if (WaveInfo.IsFormatUncompressed())
	{
		// 未压缩PCM格式的持续时间计算
		const float BytesPerSecond = static_cast<float>(*WaveInfo.pAvgBytesPerSec);
		SoundWave->Duration = BytesPerSecond > 0 ? static_cast<float>(WaveInfo.SampleDataSize) / BytesPerSecond : 0.0f;
		if (*WaveInfo.pFormatTag == FWaveModInfo::WAVE_INFO_FORMAT_PCM)
		{
			SoundWave->SetSoundAssetCompressionType(ESoundAssetCompressionType::PCM);
		}
	}
	else
	{
		// 压缩格式的持续时间可能需要特殊处理
		SoundWave->Duration = INDEFINITELY_LOOPING_DURATION;
		UE_LOG(LogTemp, Warning, TEXT("Compressed WAV format detected for %s - duration may not be accurate"),
		       *AssetName);
	}
	if (WaveInfo.WaveSampleLoops.Num() > 0)
	{
		SoundWave->bLooping = true;
	}

	FSharedBuffer SharedBuffer = FSharedBuffer::Clone(WaveDataPtr, WaveDataSize);
	SoundWave->RawData.UpdatePayload(SharedBuffer, SoundWave);

	SoundWave->SoundGroup = SOUNDGROUP_Default;
	SoundWave->DecompressionType = DTYPE_Setup;
	SoundWave->bProcedural = false;
	SoundWave->bStreaming = false;

	SoundWave->InvalidateCompressedData(true, true);

	SoundWave->PostEditChange();

	UE_LOG(LogTemp, Log,
	       TEXT("Successfully created SoundWave: %s (SampleRate: %d, Channels: %d, Duration: %.2fs, Format: %d)"),
	       *AssetName, *WaveInfo.pSamplesPerSec, *WaveInfo.pChannels, SoundWave->Duration, *WaveInfo.pFormatTag);

	return SoundWave;
}

TArray<uint8> FCoDAssetHelper::BuildDDSHeader(uint32 Width, uint32 Height, uint32 Depth, uint32 MipLevels,
                                              uint8 DxgiFormat, bool bIsCubeMap)
{
	DirectX::TexMetadata Metadata;
	Metadata.width = Width;
	Metadata.height = Height;
	Metadata.depth = 1;
	Metadata.arraySize = bIsCubeMap ? 6 : 1;
	Metadata.mipLevels = MipLevels;
	Metadata.miscFlags = bIsCubeMap ? DirectX::TEX_MISC_FLAG::TEX_MISC_TEXTURECUBE : 0;
	Metadata.miscFlags2 = 0;
	Metadata.dimension = DirectX::TEX_DIMENSION::TEX_DIMENSION_TEXTURE2D;
	Metadata.format = static_cast<DXGI_FORMAT>(DxgiFormat);

	TArray<uint8> HeaderData;
	uint32 HeaderSize = sizeof(uint32_t) + sizeof(DirectX::DDS_HEADER) + sizeof(DirectX::DDS_HEADER_DXT10);
	HeaderData.AddUninitialized(HeaderSize);

	size_t RequiredSizeBytes = 0;
	HRESULT hr = DirectX::EncodeDDSHeader(Metadata, DirectX::DDS_FLAGS::DDS_FLAGS_NONE, HeaderData.GetData(),
	                                      HeaderSize, RequiredSizeBytes);
	if (FAILED(hr))
	{
		UE_LOG(LogTemp, Error, TEXT("BuildDDSHeader: Failed to encode DDS header. HRESULT: 0x%X"), hr);
		return {};
	}
	if (RequiredSizeBytes != HeaderData.Num())
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("BuildDDSHeader: Encoded header size (%zu) differs from initial calculation (%d). Resizing."),
		       RequiredSizeBytes, HeaderData.Num());
		HeaderData.SetNum(RequiredSizeBytes);
	}
	return HeaderData;
}

uint8 FCoDMeshHelper::FindFaceIndex(TSharedPtr<IMemoryReader>& MemoryReader, uint64 PackedIndices, uint32 Index,
                                    uint8 Bits, bool IsLocal)
{
	unsigned long BitIndex;

	if (!_BitScanReverse64(&BitIndex, Bits)) BitIndex = 64;
	else BitIndex ^= 0x3F;

	const uint16 Offset = Index * (64 - BitIndex);
	const uint8 BitCount = 64 - BitIndex;
	const uint64 PackedIndicesPtr = PackedIndices + (Offset >> 3);
	const uint8 BitOffset = Offset & 7;

	uint8 PackedIndice;
	MemoryReader->ReadMemory<uint8>(PackedIndicesPtr, PackedIndice, IsLocal);

	if (BitOffset == 0)
		return PackedIndice & ((1 << BitCount) - 1);

	if (8 - BitOffset < BitCount)
	{
		uint8 nextPackedIndice;
		MemoryReader->ReadMemory<uint8>(PackedIndicesPtr + 1, nextPackedIndice, IsLocal);
		return (PackedIndice >> BitOffset) & ((1 << (8 - BitOffset)) - 1) | ((nextPackedIndice & ((1 << (64 -
			BitIndex - (8 - BitOffset))) - 1)) << (8 - BitOffset));
	}

	return (PackedIndice >> BitOffset) & ((1 << BitCount) - 1);
}

bool FCoDMeshHelper::UnpackFaceIndices(TSharedPtr<IMemoryReader>& MemoryReader, TArray<uint16>& InFacesArr,
                                       uint64 Tables, uint64 TableCount, uint64 PackedIndices, uint64 Indices,
                                       uint64 FaceIndex, const bool IsLocal)
{
	InFacesArr.SetNum(3);
	uint64 CurrentFaceIndex = FaceIndex;
	for (int i = 0; i < TableCount; i++)
	{
		uint64 TablePtr = Tables + (i * 40);
		uint32 IndicesPtrOffset;
		MemoryReader->ReadMemory<uint32>(TablePtr + 36, IndicesPtrOffset, IsLocal);
		uint64 IndicesPtr = PackedIndices + IndicesPtrOffset;
		uint8 Count;
		MemoryReader->ReadMemory<uint8>(TablePtr + 35, Count, IsLocal);
		if (CurrentFaceIndex < Count)
		{
			uint8 Bits;
			MemoryReader->ReadMemory<uint8>(TablePtr + 34, Bits, IsLocal);
			Bits -= 1;

			uint32 MemoryFaceIdx;
			MemoryReader->ReadMemory<uint32>(TablePtr + 28, MemoryFaceIdx, IsLocal);
			FaceIndex = MemoryFaceIdx;

			uint32 Face1Offset = FindFaceIndex(MemoryReader, IndicesPtr, CurrentFaceIndex * 3 + 0, Bits, IsLocal) +
				FaceIndex;
			uint32 Face2Offset = FindFaceIndex(MemoryReader, IndicesPtr, CurrentFaceIndex * 3 + 1, Bits, IsLocal) +
				FaceIndex;
			uint32 Face3Offset = FindFaceIndex(MemoryReader, IndicesPtr, CurrentFaceIndex * 3 + 2, Bits, IsLocal) +
				FaceIndex;

			MemoryReader->ReadMemory<uint16>(Indices + Face1Offset * 2, InFacesArr[0], IsLocal);
			MemoryReader->ReadMemory<uint16>(Indices + Face2Offset * 2, InFacesArr[1], IsLocal);
			MemoryReader->ReadMemory<uint16>(Indices + Face3Offset * 2, InFacesArr[2], IsLocal);
			return true;
		}
		CurrentFaceIndex -= Count;
	}
	return false;
}

void FCoDMeshHelper::UnpackCoDQTangent(const uint32 Packed, FVector3f& Tangent, FVector3f& Normal)
{
	uint32 Idx = Packed >> 30;

	float TX = ((Packed >> 00 & 0x3FF) / 511.5f - 1.0f) / 1.4142135f;
	float TY = ((Packed >> 10 & 0x3FF) / 511.5f - 1.0f) / 1.4142135f;
	float TZ = ((Packed >> 20 & 0x1FF) / 255.5f - 1.0f) / 1.4142135f;
	float TW = 0.0f;
	float Sum = TX * TX + TY * TY + TZ * TZ;

	if (Sum <= 1.0f) TW = FMath::Sqrt(1.0f - Sum);

	float QX = 0.0f;
	float QY = 0.0f;
	float QZ = 0.0f;
	float QW = 0.0f;

	auto SetVal = [&QX,&QY,&QW,&QZ](float& B1, float& B2, float& B3, float& B4)
	{
		QX = B1;
		QY = B2;
		QZ = B3;
		QW = B4;
	};

	switch (Idx)
	{
	case 0:
		SetVal(TW, TX, TY, TZ);
		break;
	case 1:
		SetVal(TX, TW, TY, TZ);
		break;
	case 2:
		SetVal(TX, TY, TW, TZ);
		break;
	case 3:
		SetVal(TX, TY, TZ, TW);
		break;
	default:
		break;
	}

	Tangent = FVector3f(1 - 2 * (QY * QY + QZ * QZ), 2 * (QX * QY + QW * QZ), 2 * (QX * QZ - QW * QY));

	FVector3f Bitangent(2 * (QX * QY - QW * QZ), 1 - 2 * (QX * QX + QZ * QZ), 2 * (QY * QZ + QW * QX));

	Normal = Tangent.Cross(Bitangent);
}

FVector4 VectorPacking::QuatPackingA(const uint64 PackedData)
{
	uint64 PackedQuatData = PackedData;
	int32 Axis = PackedQuatData & 3;
	int32 WSign = PackedQuatData >> 63;
	PackedQuatData >>= 2;

	int32 ix = (int32)(PackedQuatData & 0xfffff);
	if (ix > 0x7ffff) ix -= 0x100000;
	int32 iy = (int32)((PackedQuatData >> 20) & 0xfffff);
	if (iy > 0x7ffff) iy -= 0x100000;
	int32 iz = (int32)((PackedQuatData >> 40) & 0xfffff);
	if (iz > 0x7ffff) iz -= 0x100000;
	float x = ix / 1048575.f;
	float y = iy / 1048575.f;
	float z = iz / 1048575.f;

	x *= 1.41421f;
	y *= 1.41421f;
	z *= 1.41421f;

	float w = (float)std::pow<float>(1 - x * x - y * y - z * z, 0.5f);

	if (WSign)
		w = -w;

	switch (Axis)
	{
	case 0: return FVector4(w, x, y, z);
	case 1: return FVector4(x, y, z, w);
	case 2: return FVector4(y, z, w, x);
	case 3: return FVector4(z, w, x, y);
	default: return FVector4();
	}
}

FVector4 VectorPacking::QuatPacking2DA(const uint32 PackedData)
{
	uint32 PackedQuatData = PackedData;
	int WSign = ((PackedQuatData >> 30) & 1);
	PackedQuatData &= 0xBFFFFFFF;

	float Z = *(float*)&PackedQuatData;
	float W = (float)std::sqrtf(1.f - std::pow<float>(Z, 2.f));

	if (WSign)
		W = -W;

	return FVector4(0, 0, Z, W);
}

FString FCoDAssetNameHelper::NoIllegalSigns(const FString& InString)
{
	FString SanitizedString;
	for (TCHAR Char : InString)
	{
		if (FChar::IsAlnum(Char) || Char == TEXT('_'))
		{
			SanitizedString.AppendChar(Char);
		}
		else
		{
			SanitizedString.AppendChar(TEXT('_'));
		}
	}
	return SanitizedString;
}

uint16 HalfFloatHelper::ToHalfFloat(float Value)
{
	FFloatBits V, S;
	V.F = Value;

	uint32 Sign = V.SI & SignN;
	V.SI ^= Sign;
	Sign >>= ShiftSign;

	S.SI = MulN;
	S.SI = static_cast<int32>(S.F * V.F);

	V.SI ^= (S.SI ^ V.SI) & -(MinN > V.SI);
	V.SI ^= (InfN ^ V.SI) & -((InfN > V.SI) & (V.SI > MaxN));
	V.SI ^= (NanN ^ V.SI) & -((NanN > V.SI) & (V.SI > InfN));

	V.UI >>= Shift;
	V.SI ^= ((V.SI - MaxD) ^ V.SI) & -(V.SI > MaxC);
	V.SI ^= ((V.SI - MinD) ^ V.SI) & -(V.SI > SubC);

	return static_cast<uint16>(V.UI | Sign);
}

float HalfFloatHelper::ToFloat(uint16 Value)
{
	FFloatBits V, S;
	V.UI = Value;

	int32 Sign = V.SI & SignC;
	V.SI ^= Sign;
	Sign <<= ShiftSign;

	V.SI ^= ((V.SI + MinD) ^ V.SI) & -(V.SI > SubC);
	V.SI ^= ((V.SI + MaxD) ^ V.SI) & -(V.SI > MaxC);

	S.SI = MulC;
	S.F *= V.SI;

	int32 Mask = -(NorC > V.SI);
	V.SI <<= Shift;
	V.SI ^= (S.SI ^ V.SI) & Mask;
	V.SI |= Sign;

	return V.F;
}
