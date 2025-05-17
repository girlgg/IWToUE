#pragma once
#include "CastAnimation.h"
#include "Interface/ICastAssetImporter.h"
#include "Widgets/CastImportUI.h"

struct FCastCurveInfo;
struct FCastAnimationInfo;

class FDefaultCastAnimationImporter : public ICastAnimationImporter
{
public:
	virtual UAnimSequence* ImportAnimation(const FCastAnimationInfo& AnimInfo, const FCastImportOptions& Options,
	                                       USkeleton* Skeleton, UObject* InParent, FName Name,
	                                       EObjectFlags Flags) override;

private:
	struct FPreparedBoneTrack
	{
		FName BoneName;
		TArray<FVector3f> Positions;
		TArray<FQuat4f> Rotations;
		TArray<FVector3f> Scales;
	};
	struct FPreparedNotify
	{
		FName NotifyName;
		float Time;
	};
	struct FInterpolatedBoneCurve
	{
		TArray<float> PositionX, PositionY, PositionZ;
		TArray<FQuat4f> Rotation;
		TArray<float> ScaleX, ScaleY, ScaleZ;
		ECastAnimImportType AnimMode = ECastAnimImportType::CastAIT_Absolutely;

		void PadToLength(uint32 Length, FVector3f DefaultPos, FQuat4f DefaultRot, FVector3f DefaultScale);
	};

	TArray<FPreparedBoneTrack> PrepareBoneTrackData(const USkeleton* Skeleton,
	                                                const TMap<FString, FInterpolatedBoneCurve>& BoneCurves,
	                                                const FCastImportOptions& Options, uint32 NumberOfFrames) const;
	TArray<FPreparedNotify> PrepareAnimationNotifies(const FCastAnimationInfo& Animation) const;
	
	void InitializeAnimationController(IAnimationDataController& Controller, const FCastAnimationInfo& Animation,
	                                   uint32 NumberOfFrames);
	uint32 CalculateNumberOfFrames(const FCastAnimationInfo& Animation);
	TMap<FString, FInterpolatedBoneCurve> ExtractAndInterpolateCurves(const USkeleton* Skeleton,
	                                                                  const FCastAnimationInfo& Animation,
	                                                                  uint32 NumberOfFrames,
	                                                                  bool bSkipRootAnim);
	void PopulateBoneTracks(IAnimationDataController& Controller, const TArray<FPreparedBoneTrack>& PreparedTracks);
	void AddAnimationNotifies(UAnimSequence* DestSeq, const TArray<FPreparedNotify>& PreparedNotifies);
	void FinalizeController(IAnimationDataController& Controller, UAnimSequence* DestSeq);

	template <typename T>
	T Interpolate(const TArray<uint32>& Frames, const TArray<T>& Values, uint32 TargetFrame);
	template <typename T>
	void InterpolateCurve(TArray<T>& OutValues, const FCastCurveInfo& Curve, uint32 NumberOfFrames);
};

template <typename T>
T FDefaultCastAnimationImporter::Interpolate(const TArray<uint32>& Frames, const TArray<T>& Values, uint32 TargetFrame)
{
	if (Frames.IsEmpty() || Values.IsEmpty()) return T();
	if (Frames.Num() != Values.Num()) return T();

	int32 LowerBoundIdx = Algo::LowerBound(Frames, TargetFrame);
	int32 PrevFrameIdx = LowerBoundIdx - 1;
	int32 NextFrameIdx = LowerBoundIdx;

	if (PrevFrameIdx < 0) return Values[0];
	if (NextFrameIdx >= Frames.Num()) return Values.Last();

	uint32 FrameA = Frames[PrevFrameIdx];
	uint32 FrameB = Frames[NextFrameIdx];
	if (FrameA == FrameB) return Values[PrevFrameIdx];

	float Alpha = (float)(TargetFrame - FrameA) / (float)(FrameB - FrameA);

	if constexpr (std::is_same_v<T, float>)
	{
		return FMath::Lerp(Values[PrevFrameIdx], Values[NextFrameIdx], Alpha);
	}
	else if constexpr (std::is_same_v<T, FQuat4f>)
	{
		return FQuat4f::Slerp(Values[PrevFrameIdx], Values[NextFrameIdx], Alpha).GetNormalized();
	}
	else if constexpr (std::is_same_v<T, FVector4f>)
	{
		return FMath::Lerp(Values[PrevFrameIdx], Values[NextFrameIdx], Alpha);
	}
	else
	{
		return Values[PrevFrameIdx];
	}
}

template <typename T>
void FDefaultCastAnimationImporter::InterpolateCurve(TArray<T>& OutValues, const FCastCurveInfo& Curve,
                                                     uint32 NumberOfFrames)
{
	OutValues.Empty(NumberOfFrames);
	if (Curve.KeyFrameBuffer.IsEmpty() || Curve.KeyValueBuffer.IsEmpty() || Curve.KeyFrameBuffer.Num() != Curve.
		KeyValueBuffer.Num())
	{
		UE_LOG(LogCast, Warning, TEXT("Invalid or empty keyframe data for curve %s:%s. Skipping interpolation."),
		       *Curve.NodeName, *Curve.KeyPropertyName);
		OutValues.Init(T(), NumberOfFrames);
		return;
	}

	TArray<FQuat4f> QuatValues;
	if (Curve.KeyPropertyName == TEXT("rq"))
	{
		QuatValues.Reserve(Curve.KeyValueBuffer.Num());
		for (const FVariant& Var : Curve.KeyValueBuffer)
		{
			if (Var.GetType() == EVariantTypes::Vector4)
			{
				const FVector4& Vec = Var.GetValue<FVector4>();
				QuatValues.Add(FQuat4f(Vec.X, -Vec.Y, Vec.Z, -Vec.W).GetNormalized());
			}
		}
	}

	if constexpr (std::is_same_v<T, float>)
	{
		TArray<float> FloatValues;
		FloatValues.Reserve(Curve.KeyValueBuffer.Num());
		for (const FVariant& Var : Curve.KeyValueBuffer)
		{
			if (Var.GetType() == EVariantTypes::Float)
			{
				FloatValues.Add(Var.GetValue<float>());
			}
		}
		for (uint32 Frame = 0; Frame < NumberOfFrames; ++Frame)
		{
			OutValues.Add(Interpolate(Curve.KeyFrameBuffer, FloatValues, Frame));
		}
	}
	else if constexpr (std::is_same_v<T, FQuat4f>)
	{
		for (uint32 Frame = 0; Frame < NumberOfFrames; ++Frame)
		{
			OutValues.Add(Interpolate(Curve.KeyFrameBuffer, QuatValues, Frame));
		}
	}
	else
	{
		check(false);
	}
}
