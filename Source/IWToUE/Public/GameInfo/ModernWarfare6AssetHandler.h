#pragma once

#include "Interface/IGameAssetHandler.h"

struct FMW6XAnimBufferState;
struct FMW6XModel;
struct FCastRoot;

class FModernWarfare6AssetHandler : public IGameAssetHandler
{
public:
	explicit FModernWarfare6AssetHandler(const TSharedPtr<FGameProcess>& InGameProcess)
		: IGameAssetHandler(InGameProcess)
	{
	}

	virtual ~FModernWarfare6AssetHandler() override = default;

	virtual bool ReadModelData(TSharedPtr<FCoDModel> ModelInfo, FWraithXModel& OutModel) override;
	virtual bool ReadAnimData(TSharedPtr<FCoDAnim> AnimInfo, FWraithXAnim& OutAnim) override;
	virtual bool ReadImageData(TSharedPtr<FCoDImage> ImageInfo, TArray<uint8>& OutImageData, uint8& OutFormat) override;
	virtual bool ReadImageDataFromPtr(uint64 ImageHandle, TArray<uint8>& OutImageData, uint8& OutFormat) override;
	virtual bool ReadImageDataToTexture(uint64 ImageHandle, UTexture2D*& OutTexture, FString& ImageName,
	                                    FString ImagePath = TEXT("")) override;
	virtual bool ReadSoundData(TSharedPtr<FCoDSound> SoundInfo, FWraithXSound& OutSound) override;
	virtual bool ReadMaterialData(TSharedPtr<FCoDMaterial> MaterialInfo, FWraithXMaterial& OutMaterial) override;
	virtual bool ReadMaterialDataFromPtr(uint64 MaterialHandle, FWraithXMaterial& OutMaterial) override;
	virtual bool ReadMapData(TSharedPtr<FCoDMap> MapInfo, FWraithXMap& OutMapData) override;
	
	virtual bool TranslateAnim(const FWraithXAnim& InAnim, FCastAnimationInfo& OutAnimInfo) override;

	virtual bool LoadStreamedModelData(const FWraithXModel& InModel, FWraithXModelLod& InOutLod,
	                                   FCastModelInfo& OutModelInfo) override;
	virtual bool LoadStreamedAnimData(const FWraithXAnim& InAnim, FCastAnimationInfo& OutAnimInfo) override;

	virtual void LoadXModel(FWraithXModel& InModel, FWraithXModelLod& ModelLod, FCastModelInfo& OutModel) override;

protected:
	void LoadXAnim(const FWraithXAnim& InAnim, FCastAnimationInfo& OutAnim);

	void MW6XAnimCalculateBufferIndex(FMW6XAnimBufferState& AnimState, const int32 TableSize,
	                                  const int32 KeyFrameIndex);
	static int32 MW6XAnimCalculateBufferOffset(const FMW6XAnimBufferState& AnimState, const int32 Index,
	                                           const int32 Count);

	void DeltaTranslations64(FCastAnimationInfo& OutAnim, uint32 FrameSize, const FWraithXAnim& InAnim);
	void Delta2DRotations64(FCastAnimationInfo& OutAnim, uint32 FrameSize, const FWraithXAnim& InAnim);
	void Delta3DRotations64(FCastAnimationInfo& OutAnim, uint32 FrameSize, const FWraithXAnim& InAnim);

	static FVector4 ProcessRaw2DRotation(const FQuat2Data& RawRotData, const FWraithXAnim& InAnim);
	static FVector4 ProcessRaw3DRotation(const FQuatData& RawRotData, const FWraithXAnim& InAnim);

	uint32 ReadFrameIndexData(uint64& Ptr, uint32 FrameSize);

	template <typename T>
	static void MW6XAnimIncrementBuffers(FMW6XAnimBufferState& AnimState, int32 TableSize, const int32 ElemCount,
	                                     TArray<T*>& Buffers);

	template <typename T>
	void ReadAndAdvance(uint64& Ptr, T& OutValue, size_t BytesToAdvanceOverride = 0);

	template <typename TRawRotDataType, size_t TRawRotDataSizeBytes>
	void ProcessDeltaRotationsInternal(
		FCastAnimationInfo& OutAnim,
		uint32 FrameSize,
		const FWraithXAnim& InAnim,
		uint64 InitialRotationsPtr,
		TFunctionRef<FVector4(const TRawRotDataType&, const FWraithXAnim&)> ProcessRawRotationFuncRef
	);
	template <typename TFinalKeyType>
	void ProcessDeltaFramesAndAdd(
		FCastAnimationInfo& OutAnim,
		uint16 FrameCount,
		uint32 FrameSize,
		uint64& FrameIndexPtr,
		uint64& KeyDataStreamPtr,
		const FWraithXAnim& InAnim,
		TFunctionRef<TFinalKeyType(uint64&, const FWraithXAnim&)> ReadAndProcessFunc,
		TFunctionRef<void(FCastAnimationInfo&, const FString&, uint32, TFinalKeyType&&)> AddKeyFunc
	);
};

#include "ModernWarfare6AssetHandler.inl"
