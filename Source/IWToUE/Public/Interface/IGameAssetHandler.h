#pragma once
#include "WraithX/GameProcess.h"

class IMemoryReader;
struct FWraithXMaterial;
struct FWraithXSound;
struct FCoDModel;
struct FCoDAnim;
struct FCoDImage;
struct FCoDSound;
struct FCoDMaterial;
struct FWraithXModel;
struct FWraithXModelLod;
struct FCastModelInfo;
struct FWraithXAnim;
struct FCastAnimationInfo;
class FGameProcess;

class IGameAssetHandler
{
public:
	virtual ~IGameAssetHandler() = default;

	// --- 读取原始资产 ---

	virtual bool ReadModelData(TSharedPtr<FCoDModel> ModelInfo, FWraithXModel& OutModel) = 0;
	virtual bool ReadAnimData(TSharedPtr<FCoDAnim> AnimInfo, FWraithXAnim& OutAnim) = 0;
	virtual bool ReadImageData(TSharedPtr<FCoDImage> ImageInfo, TArray<uint8>& OutImageData, uint8& OutFormat) = 0;
	virtual bool ReadImageDataFromPtr(uint64 ImageHandle, TArray<uint8>& OutImageData, uint8& OutFormat) = 0;
	virtual bool ReadImageDataToTexture(uint64 ImageHandle, UTexture2D*& OutTexture, FString& ImageName,
	                                    FString ImagePath = TEXT("")) = 0;
	virtual bool ReadSoundData(TSharedPtr<FCoDSound> SoundInfo, FWraithXSound& OutSound) = 0;
	virtual bool ReadMaterialData(TSharedPtr<FCoDMaterial> MaterialInfo, FWraithXMaterial& OutMaterial) = 0;
	virtual bool ReadMaterialDataFromPtr(uint64 MaterialHandle, FWraithXMaterial& OutMaterial) = 0;

	// --- 数据转换 ---

	virtual bool TranslateModel(FWraithXModel& InModel, int32 LodIdx, FCastModelInfo& OutModelInfo) = 0;
	virtual bool TranslateAnim(const FWraithXAnim& InAnim, FCastAnimationInfo& OutAnimInfo) = 0;

	virtual void ApplyDeltaTranslation(FCastAnimationInfo& OutAnim, const FWraithXAnim& InAnim) = 0;
	virtual void ApplyDelta2DRotation(FCastAnimationInfo& OutAnim, const FWraithXAnim& InAnim) = 0;
	virtual void ApplyDelta3DRotation(FCastAnimationInfo& OutAnim, const FWraithXAnim& InAnim) = 0;

	// --- 流式数据传输 ---

	virtual bool LoadStreamedModelData(const FWraithXModel& InModel, FWraithXModelLod& InOutLod,
	                                   FCastModelInfo& OutModelInfo) = 0;
	virtual bool LoadStreamedAnimData(const FWraithXAnim& InAnim, FCastAnimationInfo& OutAnimInfo) = 0;

protected:
	TSharedPtr<IMemoryReader> MemoryReader;
	TSharedPtr<FGameProcess> GameProcess;

	IGameAssetHandler(const TSharedPtr<FGameProcess>& InGameProcess) : MemoryReader(InGameProcess->GetMemoryReader()),
	                                                                   GameProcess(InGameProcess)
	{
	}
};
