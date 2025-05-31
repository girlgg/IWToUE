#pragma once
#include "Interface/IGameAssetHandler.h"

class ModernWarfare5AssetHandler : public IGameAssetHandler
{
public:
	explicit ModernWarfare5AssetHandler(const TSharedPtr<FGameProcess>& InGameProcess)
		: IGameAssetHandler(InGameProcess)
	{
	}

	virtual ~ModernWarfare5AssetHandler() override = default;

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
};
