#include "Utils/CoDAssetHelper.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Interface/IMemoryReader.h"

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
		// DX解码时会自动转换srgb，这里再让UE转换会导致图像偏暗
		// bOutSRGB = DirectX::IsSRGB(DxgiFormat);
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
			OutSourceFormat = TSF_BGRA8;
			OutPixelFormat = PF_R8G8B8A8;
		// bOutSRGB = false;
			break;
		case DXGI_FORMAT_B8G8R8A8_UNORM:
			OutSourceFormat = TSF_BGRA8;
			OutPixelFormat = PF_B8G8R8A8;
		// bOutSRGB = false;
			break;
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
			OutSourceFormat = TSF_BGRA8;
			OutPixelFormat = PF_R8G8B8A8;
		// bOutSRGB = true;
			break;
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			OutSourceFormat = TSF_BGRA8;
			OutPixelFormat = PF_B8G8R8A8;
		// bOutSRGB = true;
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
			OutSourceFormat = TSF_BGRA8;
			OutPixelFormat = PF_DXT1;
			break;
		case DXGI_FORMAT_BC2_UNORM:
		case DXGI_FORMAT_BC2_UNORM_SRGB:
			OutSourceFormat = TSF_BGRA8;
			OutPixelFormat = PF_DXT3;
			break;
		case DXGI_FORMAT_BC3_UNORM:
		case DXGI_FORMAT_BC3_UNORM_SRGB:
			OutSourceFormat = TSF_BGRA8;
			OutPixelFormat = PF_DXT5;
			break;
		case DXGI_FORMAT_BC4_UNORM:
		case DXGI_FORMAT_BC4_SNORM:
			OutSourceFormat = TSF_G8;
			OutPixelFormat = PF_BC4;
		// bOutSRGB = false;
			break;
		case DXGI_FORMAT_BC5_UNORM:
		case DXGI_FORMAT_BC5_SNORM:
			OutSourceFormat = TSF_BGRA8;
			OutPixelFormat = PF_BC5;
		// bOutSRGB = false;
			break;
		case DXGI_FORMAT_BC6H_UF16:
		case DXGI_FORMAT_BC6H_SF16:
			OutSourceFormat = TSF_RGBA16F;
			OutPixelFormat = PF_BC6H;
		// bOutSRGB = false;
			break;
		case DXGI_FORMAT_BC7_UNORM:
			OutSourceFormat = TSF_BGRA8;
			OutPixelFormat = PF_BC7;
			break;
		case DXGI_FORMAT_BC7_UNORM_SRGB:
			OutSourceFormat = TSF_BGRA8;
			OutPixelFormat = PF_BC7;
		// bOutSRGB = true;
			break;
		case DXGI_FORMAT_R16_FLOAT:
			OutSourceFormat = TSF_R16F;
			OutPixelFormat = PF_R16F;
		// bOutSRGB = false;
			break;
		case DXGI_FORMAT_R16_UNORM:
			OutSourceFormat = TSF_G16;
			OutPixelFormat = PF_G16;
		// bOutSRGB = false;
			break;

		default:
			UE_LOG(LogTemp, Warning, TEXT("GetUnrealFormat: Unsupported DXGI_FORMAT: %d"),
			       static_cast<int>(DxgiFormat));
			return false;
		}

		return true;
	}

	TextureCompressionSettings GetCompressionSettingsFromDXGI(DXGI_FORMAT DxgiFormat, bool bIsSRGB)
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
		case DXGI_FORMAT_R8_UNORM:
		case DXGI_FORMAT_A8_UNORM:
			return TC_Alpha;

		case DXGI_FORMAT_BC5_UNORM:
		case DXGI_FORMAT_BC5_SNORM:
			return TC_Normalmap;

		case DXGI_FORMAT_BC6H_UF16:
		case DXGI_FORMAT_BC6H_SF16:
			return TC_HDR;

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

UTexture2D* FCoDAssetHelper::CreateTextureOnGameThread(FPreparedTextureData PreparedData)
{
	check(IsInGameThread());

	if (!PreparedData.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("CreateTextureOnGameThread: Received invalid prepared data for %s."),
		       *PreparedData.TextureName);
		return nullptr;
	}

	const FString TextureName = PreparedData.TextureName;
	const FString PackagePath = PreparedData.PackagePath;

	FString PackageName = FPaths::Combine(PackagePath, TextureName);
	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateTextureOnGameThread: Failed to create package %s for texture %s"),
		       *PackageName, *TextureName);
		return nullptr;
	}
	Package->FullyLoad();
	Package->SetDirtyFlag(true);

	UTexture2D* NewTexture = NewObject<UTexture2D>(Package, FName(*TextureName),
	                                               RF_Public | RF_Standalone | RF_MarkAsRootSet);
	if (!NewTexture)
	{
		UE_LOG(LogTemp, Error,
		       TEXT("CreateTextureOnGameThread: Failed to create UTexture2D object for %s in package %s"), *TextureName,
		       *PackageName);
		return nullptr;
	}

	NewTexture->SetPlatformData(nullptr);
	NewTexture->SRGB = PreparedData.bSRGB;
	NewTexture->CompressionSettings = PreparedData.CompressionSettings;
	NewTexture->Filter = TF_Default;
	NewTexture->AddressX = TA_Wrap;
	NewTexture->AddressY = TA_Wrap;
	NewTexture->LODGroup = TEXTUREGROUP_World;
	NewTexture->MipGenSettings = TMGS_FromTextureGroup;

	if (NewTexture->CompressionSettings == TC_Normalmap)
	{
		NewTexture->LODGroup = TEXTUREGROUP_WorldNormalMap;
		NewTexture->SRGB = false;
	}
	else if (NewTexture->CompressionSettings == TC_HDR || NewTexture->CompressionSettings == TC_HDR_Compressed)
	{
		NewTexture->SRGB = false;
	}
	else if (NewTexture->CompressionSettings == TC_Grayscale || NewTexture->CompressionSettings == TC_Alpha ||
		NewTexture->CompressionSettings == TC_Displacementmap || NewTexture->CompressionSettings ==
		TC_VectorDisplacementmap)
	{
		NewTexture->SRGB = false;
	}

	NewTexture->PreEditChange(nullptr);

	NewTexture->Source.Init(
		PreparedData.Width,
		PreparedData.Height,
		1,
		1,
		PreparedData.SourceDataFormat,
		PreparedData.Mip0Data.GetData()
	);

	uint8* DestMipData = NewTexture->Source.LockMip(0);
	SIZE_T DestMipDataSize = NewTexture->Source.CalcMipSize(0);
	SIZE_T SourceMipDataSize = PreparedData.Mip0Data.Num();

	if (!DestMipData)
	{
		UE_LOG(LogTemp, Error,
		       TEXT("CreateTextureOnGameThread: Failed to lock Mip 0 after Source.Init for texture %s"),
		       *TextureName);
		NewTexture->Source.UnlockMip(0);
		NewTexture->MarkAsGarbage();
		return nullptr;
	}

	if (DestMipDataSize != SourceMipDataSize)
	{
		UE_LOG(LogTemp, Warning,
		       TEXT(
			       "CreateTextureOnGameThread: Mip 0 data size mismatch after Source.Init for %s. Expected: %llu, Source: %llu."
		       ),
		       *TextureName, DestMipDataSize, SourceMipDataSize);
		SIZE_T CopySize = FMath::Min(DestMipDataSize, SourceMipDataSize);
		if (FMemory::Memcmp(DestMipData, PreparedData.Mip0Data.GetData(), CopySize) != 0)
		{
			UE_LOG(LogTemp, Warning,
			       TEXT("CreateTextureOnGameThread: Data mismatch after Init, attempting Memcpy for %s."),
			       *TextureName);
			FMemory::Memcpy(DestMipData, PreparedData.Mip0Data.GetData(), CopySize);
		}
	}
	else
	{
		if (FMemory::Memcmp(DestMipData, PreparedData.Mip0Data.GetData(), SourceMipDataSize) != 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("CreateTextureOnGameThread: Data mismatch after Init for %s."),
			       *TextureName);
		}
	}
	NewTexture->Source.UnlockMip(0);

	NewTexture->Modify();
	NewTexture->PostEditChange();

	Package->MarkPackageDirty();

	FAssetRegistryModule::AssetCreated(NewTexture);

	NewTexture->ClearFlags(RF_MarkAsRootSet);

	return NewTexture;
}

UTexture2D* FCoDAssetHelper::CreateTextureFromDDSData(const TArray<uint8>& DDSDataArray, const FString& TextureName,
                                                      const FString& PackagePath)
{
	// --- Step 1: Initial Checks (Any thread) ---
	if (DDSDataArray.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("CreateTextureFromDDSData: Input DDS data array is empty for %s"), *TextureName);
		return nullptr;
	}
	if (TextureName.IsEmpty() || PackagePath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("CreateTextureFromDDSData: Invalid TextureName or PackagePath provided."));
		return nullptr;
	}
	// --- Step 2: Load DDS Header and Metadata (DirectXTex, Any thread) ---
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
	// --- Step 3: Map Formats and Settings (Any thread) ---
	EPixelFormat UnrealPixelFormat = PF_Unknown;
	ETextureSourceFormat UnrealSourceFormat = TSF_Invalid;
	bool bSRGB = false;

	if (!GetUnrealFormat(Metadata.format, UnrealPixelFormat, UnrealSourceFormat, bSRGB))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to map DXGI format %d to Unreal format for texture %s"),
		       static_cast<int>(Metadata.format), *TextureName);
		return nullptr;
	}

	DXGI_FORMAT TargetDxgiFormat = DXGI_FORMAT_UNKNOWN;
	switch (UnrealSourceFormat)
	{
	case TSF_BGRA8:
		TargetDxgiFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
		break;
	case TSF_RGBA16F:
		TargetDxgiFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
		break;
	case TSF_G8:
		TargetDxgiFormat = DXGI_FORMAT_R8_UNORM;
		break;
	case TSF_RGBA32F:
		TargetDxgiFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
		break;
	case TSF_G16:
		TargetDxgiFormat = DXGI_FORMAT_R16_UNORM;
		break;
	case TSF_R16F:
		TargetDxgiFormat = DXGI_FORMAT_R16_FLOAT;
		break;
	default:
		UE_LOG(LogTemp, Error,
		       TEXT("CreateTextureFromDDSData: Cannot determine target DXGI format for Unreal Source Format %d (%s)"),
		       (int)UnrealSourceFormat, *TextureName);
		return nullptr;
	}
	// --- Step 4: Decompress or Convert Image Data if Necessary (DirectXTex, Any thread) ---
	DirectX::ScratchImage ProcessedImageData;
	const DirectX::Image* FinalImagesPtr = ScratchImage.GetImages();
	const DirectX::TexMetadata* FinalMetadataPtr = &Metadata;
	size_t FinalImageCount = ScratchImage.GetImageCount();
	bool bUsedProcessed = false;

	if (DirectX::IsCompressed(Metadata.format))
	{
		if (Metadata.format == TargetDxgiFormat)
		{
			UE_LOG(LogTemp, Warning,
			       TEXT(
				       "CreateTextureFromDDSData: DDS %s is compressed but target format is the same (%d). Skipping decompression."
			       ), *TextureName, TargetDxgiFormat);
		}
		else if (TargetDxgiFormat != DXGI_FORMAT_UNKNOWN)
		{
			hr = DirectX::Decompress(FinalImagesPtr, FinalImageCount, Metadata, TargetDxgiFormat, ProcessedImageData);
			if (FAILED(hr))
			{
				UE_LOG(LogTemp, Error,
				       TEXT(
					       "CreateTextureFromDDSData: Failed to decompress DDS image %s from format %d to %d. HRESULT: 0x%X"
				       ), *TextureName, Metadata.format, TargetDxgiFormat, hr);
				return nullptr;
			}
			FinalImagesPtr = ProcessedImageData.GetImages();
			FinalMetadataPtr = &ProcessedImageData.GetMetadata();
			FinalImageCount = ProcessedImageData.GetImageCount();
			bUsedProcessed = true;
		}
		else
		{
			UE_LOG(LogTemp, Error,
			       TEXT(
				       "CreateTextureFromDDSData: Cannot decompress %s, invalid target DXGI format derived from Unreal Source Format %d."
			       ), *TextureName, (int)UnrealSourceFormat);
			return nullptr;
		}
	}
	else if (Metadata.format != TargetDxgiFormat)
	{
		if (TargetDxgiFormat != DXGI_FORMAT_UNKNOWN)
		{
			UE_LOG(LogTemp, Verbose, TEXT("CreateTextureFromDDSData: Converting %s from %d to %d"), *TextureName,
			       Metadata.format, TargetDxgiFormat);
			hr = DirectX::Convert(FinalImagesPtr, FinalImageCount, Metadata, TargetDxgiFormat,
			                      DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, ProcessedImageData);
			if (FAILED(hr))
			{
				UE_LOG(LogTemp, Error,
				       TEXT(
					       "CreateTextureFromDDSData: Failed to convert DDS image %s from format %d to %d. HRESULT: 0x%X"
				       ),
				       *TextureName, Metadata.format, TargetDxgiFormat, hr);
				return nullptr;
			}
			FinalImagesPtr = ProcessedImageData.GetImages();
			FinalMetadataPtr = &ProcessedImageData.GetMetadata();
			FinalImageCount = ProcessedImageData.GetImageCount();
			bUsedProcessed = true;
		}
		else
		{
			UE_LOG(LogTemp, Error,
			       TEXT(
				       "CreateTextureFromDDSData: Cannot convert %s, invalid target DXGI format derived from Unreal Source Format %d."
			       ), *TextureName, (int)UnrealSourceFormat);
			return nullptr;
		}
	}
	if (!FinalImagesPtr || FinalImageCount == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateTextureFromDDSData: No valid image data available after processing for %s"),
		       *TextureName);
		return nullptr;
	}
	// --- Step 5: Extract Mip 0 Data and Prepare for Game Thread (Any thread) ---
	const size_t MipIndex = 0;
	const size_t SliceIndex = 0;
	const size_t ImageIndex = FinalMetadataPtr->ComputeIndex(MipIndex, SliceIndex, 0);
	if (ImageIndex >= FinalImageCount)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateTextureFromDDSData: Cannot find image index for Mip 0 in %s"), *TextureName);
		return nullptr;
	}
	const DirectX::Image& Mip0Image = FinalImagesPtr[ImageIndex];
	if (!Mip0Image.pixels)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateTextureFromDDSData: Mip 0 pixel data is null for %s"), *TextureName);
		return nullptr;
	}
	SIZE_T SourceMipDataSize = Mip0Image.slicePitch;
	if (Mip0Image.height > 1 && Mip0Image.rowPitch * Mip0Image.height != Mip0Image.slicePitch)
	{
		SIZE_T CalculatedSize = Mip0Image.rowPitch * Mip0Image.height;
		UE_LOG(LogTemp, Warning,
		       TEXT(
			       "CreateTextureFromDDSData: Mip 0 slicePitch (%zu) differs from rowPitch*height (%zu) for %s. Using calculated size."
		       ),
		       Mip0Image.slicePitch, CalculatedSize, *TextureName);
		SourceMipDataSize = CalculatedSize;
	}
	if (SourceMipDataSize == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateTextureFromDDSData: Calculated Mip 0 data size is zero for %s."),
		       *TextureName);
		return nullptr;
	}
	FPreparedTextureData PreparedData;
	PreparedData.TextureName = TextureName;
	PreparedData.PackagePath = PackagePath;
	PreparedData.Width = FinalMetadataPtr->width;
	PreparedData.Height = FinalMetadataPtr->height;
	PreparedData.FinalPixelFormat = UnrealPixelFormat;
	PreparedData.SourceDataFormat = UnrealSourceFormat;
	PreparedData.CompressionSettings = GetCompressionSettingsFromDXGI(Metadata.format, bSRGB);
	PreparedData.bSRGB = bSRGB;

	PreparedData.Mip0Data.SetNumUninitialized(SourceMipDataSize);
	FMemory::Memcpy(PreparedData.Mip0Data.GetData(), Mip0Image.pixels, SourceMipDataSize);

	// --- Step 6U: Dispatch to Game Thread if Necessary ---
	UTexture2D* ResultTexture = nullptr;
	if (IsInGameThread())
	{
		UE_LOG(LogTemp, Verbose, TEXT("CreateTextureFromDDSData: Executing directly on game thread for %s."),
		       *TextureName);
		ResultTexture = CreateTextureOnGameThread(MoveTemp(PreparedData));
	}
	else
	{
		UE_LOG(LogTemp, Verbose, TEXT("CreateTextureFromDDSData: Dispatching to game thread for %s."), *TextureName);
		TPromise<UTexture2D*> Promise;
		TFuture<UTexture2D*> Future = Promise.GetFuture();

		AsyncTask(ENamedThreads::GameThread,
		          [PreparedData = MoveTemp(PreparedData), Promise = MoveTemp(Promise)]() mutable
		          {
			          UTexture2D* Texture = CreateTextureOnGameThread(MoveTemp(PreparedData));
			          Promise.SetValue(Texture);
		          });

		ResultTexture = Future.Get();
		UE_LOG(LogTemp, Verbose, TEXT("CreateTextureFromDDSData: Game thread task completed for %s."), *TextureName);
	}

	return ResultTexture;
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
		if (!Outer)
		{
			UE_LOG(LogTemp, Error,
			       TEXT("Cannot create SoundWave '%s': Failed to get Transient Package and Outer was null."),
			       *AssetName);
			return nullptr;
		}
	}
	if (!IsValid(Outer))
	{
		UE_LOG(LogTemp, Error, TEXT("Cannot create SoundWave '%s': Provided or determined Outer object is invalid."),
		       *AssetName);
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
		const uint32 Channels = WaveInfo.pChannels ? *WaveInfo.pChannels : 0;
		const uint32 SampleRate = WaveInfo.pSamplesPerSec ? *WaveInfo.pSamplesPerSec : 0;
		UE_LOG(LogTemp, Error,
		       TEXT(
			       "Unsupported WAV format for sound '%s'. FormatTag: %d, BitsPerSample: %d, Channels: %d, SampleRate: %d"
		       ),
		       *AssetName, FormatTag, BitsPerSample, Channels, SampleRate);
		return nullptr;
	}

	FSharedBuffer SharedBuffer = FSharedBuffer::Clone(WaveDataPtr, WaveDataSize);
	FName SoundFName(*AssetName);
	uint32 NumChannels = *WaveInfo.pChannels;
	uint32 SampleRate = *WaveInfo.pSamplesPerSec;
	uint32 RawPCMDataSize = WaveInfo.SampleDataSize;
	float Duration = 0.0f;
	bool bLooping = WaveInfo.WaveSampleLoops.Num() > 0;
	ESoundAssetCompressionType CompressionType = ESoundAssetCompressionType::PCM;

	if (WaveInfo.IsFormatUncompressed())
	{
		// 未压缩PCM格式的持续时间计算
		const float BytesPerSecond = static_cast<float>(*WaveInfo.pAvgBytesPerSec);
		Duration = BytesPerSecond > 0 ? static_cast<float>(WaveInfo.SampleDataSize) / BytesPerSecond : 0.0f;
	}
	else
	{
		// 压缩格式的持续时间可能需要特殊处理
		Duration = INDEFINITELY_LOOPING_DURATION;
		UE_LOG(LogTemp, Warning, TEXT("Compressed WAV format detected for %s - duration may not be accurate"),
		       *AssetName);
	}

	TPromise<USoundWave*> Promise;
	TFuture<USoundWave*> Future = Promise.GetFuture();

	AsyncTask(ENamedThreads::GameThread,
	          [Outer, SoundFName, ObjectFlags, SharedBuffer, NumChannels, SampleRate, RawPCMDataSize, Duration, bLooping
		          , CompressionType, &Promise]()
	          {
		          USoundWave* SoundWave = nullptr;
		          FString ErrorLog;

		          if (!IsValid(Outer))
		          {
			          ErrorLog = FString::Printf(
				          TEXT("Outer object became invalid before game thread execution for '%s'."),
				          *SoundFName.ToString());
			          UE_LOG(LogTemp, Error, TEXT("%s"), *ErrorLog);
			          Promise.SetValue(nullptr);
			          return;
		          }

		          // --- Game Thread UObject Operations ---
		          SoundWave = NewObject<USoundWave>(Outer, SoundFName, ObjectFlags | RF_Transactional);

		          if (SoundWave)
		          {
			          SoundWave->NumChannels = NumChannels;
			          SoundWave->SetSampleRate(SampleRate);
			          SoundWave->RawPCMDataSize = RawPCMDataSize;
			          SoundWave->Duration = Duration;
			          SoundWave->bLooping = bLooping;

			          SoundWave->RawData.UpdatePayload(SharedBuffer, SoundWave);

			          SoundWave->SetSoundAssetCompressionType(CompressionType);
			          SoundWave->SoundGroup = SOUNDGROUP_Default;
			          SoundWave->DecompressionType = DTYPE_Setup;
			          SoundWave->bProcedural = false;
			          SoundWave->bStreaming = false;

			          SoundWave->InvalidateCompressedData(true, true);

			          SoundWave->PostEditChange();

			          UE_LOG(LogTemp, Log, TEXT("Successfully created and configured SoundWave '%s' on Game Thread."),
			                 *SoundFName.ToString());
		          }
		          else
		          {
			          ErrorLog = FString::Printf(
				          TEXT("Failed to create USoundWave object named '%s' in Outer '%s' on Game Thread."),
				          *SoundFName.ToString(), *Outer->GetPathName());
			          UE_LOG(LogTemp, Error, TEXT("%s"), *ErrorLog);
		          }

		          Promise.SetValue(SoundWave);
	          });

	USoundWave* ResultSoundWave = Future.Get();

	if (ResultSoundWave)
	{
		UE_LOG(LogTemp, Log, TEXT("CreateSoundWaveFromData returning successfully created SoundWave: %s"),
		       *ResultSoundWave->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateSoundWaveFromData returning nullptr for asset: %s"), *AssetName);
	}

	return ResultSoundWave;
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
