#pragma once

#include "Interface/IGameAssetHandler.h"

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

	virtual bool TranslateModel(FWraithXModel& InModel, int32 LodIdx, FCastModelInfo& OutModelInfo) override;
	virtual bool TranslateAnim(const FWraithXAnim& InAnim, FCastAnimationInfo& OutAnimInfo) override;
	virtual void ApplyDeltaTranslation(FCastAnimationInfo& OutAnim, const FWraithXAnim& InAnim) override;
	virtual void ApplyDelta2DRotation(FCastAnimationInfo& OutAnim, const FWraithXAnim& InAnim) override;
	virtual void ApplyDelta3DRotation(FCastAnimationInfo& OutAnim, const FWraithXAnim& InAnim) override;

	virtual bool LoadStreamedModelData(const FWraithXModel& InModel, FWraithXModelLod& InOutLod,
	                                   FCastModelInfo& OutModelInfo) override;
	virtual bool LoadStreamedAnimData(const FWraithXAnim& InAnim, FCastAnimationInfo& OutAnimInfo) override;

protected:
	void LoadXModel(FWraithXModel& InModel, FWraithXModelLod& ModelLod, FCastModelInfo& OutModel);
};
