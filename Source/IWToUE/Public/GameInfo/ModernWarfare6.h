#pragma once
#include "BaseGame.h"
#include "Structures/MW6GameStructures.h"
#include "Structures/SharedStructures.h"
#include "WraithX/CoDAssetType.h"

struct FCastAnimationInfo;
struct FCoDModel;
class FGameProcess;
struct FCoDImage;
struct FCodXImage;

class FModernWarfare6 : public TBaseGame<FMW6GfxWorld, FMW6GfxWorldTransientZone, FMW6GfxWorldSurfaces, FMW6GfxSurface,
                                         FMW6GfxUgbSurfData, FMW6Material, FMW6GfxWorldStaticModels,
                                         FMW6GfxStaticModelCollection, FMW6GfxStaticModel,
                                         FMW6XModel, FMW6GfxSModelInstanceData, FMW6GfxWorldDrawOffset,
                                         FMW6GfxWorldDrawVerts, FMW6XModelLod,
                                         FMW6XModelSurfs, FMW6XSurfaceShared, FMW6XSurface>
{
public:
	FModernWarfare6(FCordycepProcess* InProcess);

	virtual TArray<FCastTextureInfo> PopulateMaterial(const FMW6Material& Material) override;
	virtual float GetSurfaceScale(const FMW6XSurface& Surface) override;
	virtual FVector3f GetSurfaceOffset(const FMW6XSurface& Surface) override;

	virtual uint64 GetXPakKey(const FMW6XModelSurfs& XModelSurfs, const FMW6XSurfaceShared& Shared) override;

	virtual uint64 GetXyzOffset(const FMW6XSurface& Surface) override;
	virtual uint64 GetTangentFrameOffset(const FMW6XSurface& Surface) override;
	virtual uint64 GetTexCoordOffset(const FMW6XSurface& Surface) override;
	virtual uint64 GetColorOffset(const FMW6XSurface& Surface) override;
	virtual uint64 GetIndexDataOffset(const FMW6XSurface& Surface) override;
	virtual uint64 GetPackedIndicesOffset(const FMW6XSurface& Surface) override;

	static bool ReadXAnim(FWraithXAnim& OutAnim, TSharedPtr<FGameProcess> ProcessInstance,
	                      TSharedPtr<FCoDAnim> AnimAddr);
	static uint8 ReadXImage(TSharedPtr<FGameProcess> ProcessInstance, TSharedPtr<FCoDImage> ImageAddr,
	                        TArray<uint8>& ImageDataArray);
	static bool ReadXModel(FWraithXModel& OutModel, TSharedPtr<FGameProcess> ProcessInstance,
	                       TSharedPtr<FCoDModel> ModelAddr);
	static bool ReadXMaterial(FWraithXMaterial& OutMaterial, TSharedPtr<FGameProcess> ProcessInstance,
	                          uint64 MaterialHandle);
	static bool ReadXSound(FWraithXSound& OutSound, TSharedPtr<FGameProcess> ProcessInstance,
	                       TSharedPtr<FCoDAsset> InSound);

	static void LoadXAnim(TSharedPtr<FGameProcess> ProcessInstance, FWraithXAnim& InAnim, FCastAnimationInfo& OutAnim);
	static void LoadXModel(TSharedPtr<FGameProcess> ProcessInstance, FWraithXModel& BaseModel,
	                       FWraithXModelLod& ModelLod, FCastModelInfo& ResultModel);

	static int32 MW6XAnimCalculateBufferOffset(const FMW6XAnimBufferState* AnimState, const int32 Index,
	                                           const int32 Count);
	static void MW6XAnimCalculateBufferIndex(FMW6XAnimBufferState* AnimState, const int32 TableSize,
	                                         const int32 KeyFrameIndex);
	template <typename T>
	static void MW6XAnimIncrementBuffers(FMW6XAnimBufferState* AnimState, int32 TableSize, const int32 elemCount,
	                                     TArray<T*>& Buffers);
};

template <typename T>
void FModernWarfare6::MW6XAnimIncrementBuffers(FMW6XAnimBufferState* AnimState, int32 TableSize, const int32 ElemCount,
                                               TArray<T*>& Buffers)
{
	if (TableSize < 4 || AnimState->OffsetCount == 1)
	{
		Buffers[0] += ElemCount * TableSize;
	}
	else
	{
		for (int32 i = 0; i < AnimState->OffsetCount; i++)
		{
			Buffers[i] += ElemCount * MW6XAnimCalculateBufferOffset(AnimState, AnimState->PackedPerFrameOffset,
			                                                        TableSize);
			AnimState->PackedPerFrameOffset += TableSize;
		}
	}
}
