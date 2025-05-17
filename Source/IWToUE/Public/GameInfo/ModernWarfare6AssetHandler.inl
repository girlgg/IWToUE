#pragma once
#include "CastManager/CastAnimation.h"
#include "Interface/IMemoryReader.h"
#include "Structures/MW6GameStructures.h"

template <typename T>
void FModernWarfare6AssetHandler::MW6XAnimIncrementBuffers(FMW6XAnimBufferState& AnimState, int32 TableSize,
                                                           const int32 ElemCount, TArray<T*>& Buffers)
{
	if (TableSize < 4 || AnimState.OffsetCount == 1)
	{
		Buffers[0] += ElemCount * TableSize;
	}
	else
	{
		for (int32 i = 0; i < AnimState.OffsetCount; i++)
		{
			Buffers[i] += ElemCount * MW6XAnimCalculateBufferOffset(AnimState, AnimState.PackedPerFrameOffset,
			                                                        TableSize);
			AnimState.PackedPerFrameOffset += TableSize;
		}
	}
}

template <typename T>
void FModernWarfare6AssetHandler::ReadAndAdvance(uint64& Ptr, T& OutValue, size_t BytesToAdvanceOverride)
{
	MemoryReader->ReadMemory(Ptr, OutValue);
	Ptr += (BytesToAdvanceOverride > 0 ? BytesToAdvanceOverride : sizeof(T));
}

template <typename TRawRotDataType, size_t TRawRotDataSizeBytes>
void FModernWarfare6AssetHandler::ProcessDeltaRotationsInternal(
	FCastAnimationInfo& OutAnim,
	uint32 FrameSize,
	const FWraithXAnim& InAnim,
	uint64 InitialRotationsPtr,
	TFunctionRef<FVector4(const TRawRotDataType&, const FWraithXAnim&)> ProcessRawRotationFuncRef
)
{
	uint64 CurrentPtr = InitialRotationsPtr;
	uint16 FrameCount;
	// Advance 2 bytes, and 6 padding bytes
	ReadAndAdvance(CurrentPtr, FrameCount, 8);

	if (!FrameCount)
	{
		TRawRotDataType RotationData;
		ReadAndAdvance(CurrentPtr, RotationData, TRawRotDataSizeBytes);
		FVector4 Rotation = ProcessRawRotationFuncRef(RotationData, InAnim);
		OutAnim.AddRotationKey(TEXT("tag_origin"), 0, MoveTemp(Rotation));
		return;
	}

	uint64 DeltaDataPtr;
	ReadAndAdvance(CurrentPtr, DeltaDataPtr);

	ProcessDeltaFramesAndAdd<FVector4>(
		OutAnim, FrameCount, FrameSize, CurrentPtr, DeltaDataPtr, InAnim,
		[&](uint64& KeyStreamPtr, const FWraithXAnim& AnimParam) -> FVector4
		{
			TRawRotDataType RawData;
			ReadAndAdvance(KeyStreamPtr, RawData, TRawRotDataSizeBytes);
			return ProcessRawRotationFuncRef(RawData, AnimParam);
		},
		[](FCastAnimationInfo& Anim, const FString& Name, uint32 Index, FVector4&& Key)
		{
			Anim.AddRotationKey(Name, Index, MoveTemp(Key));
		}
	);
}

template <typename TFinalKeyType>
void FModernWarfare6AssetHandler::ProcessDeltaFramesAndAdd(
	FCastAnimationInfo& OutAnim,
	uint16 FrameCount,
	uint32 FrameSize,
	uint64& FrameIndexPtr,
	uint64& KeyDataStreamPtr,
	const FWraithXAnim& InAnim,
	TFunctionRef<TFinalKeyType(uint64&, const FWraithXAnim&)> ReadAndProcessFunc,
	TFunctionRef<void(FCastAnimationInfo&, const FString&, uint32, TFinalKeyType&&)> AddKeyFunc
)
{
	for (uint32 i = 0; i <= FrameCount; ++i)
	{
		const uint32 FrameIndex = ReadFrameIndexData(FrameIndexPtr, FrameSize);
		TFinalKeyType FinalKey = ReadAndProcessFunc(KeyDataStreamPtr, InAnim);
		AddKeyFunc(OutAnim, TEXT("tag_origin"), FrameIndex, MoveTemp(FinalKey));
	}
}
