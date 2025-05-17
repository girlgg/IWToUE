#include "CastManager/DefaultCastAnimationImporter.h"

#include "CastManager/CastAnimation.h"
#include "CastManager/CastImportOptions.h"

UAnimSequence* FDefaultCastAnimationImporter::ImportAnimation(const FCastAnimationInfo& AnimInfo,
                                                              const FCastImportOptions& Options, USkeleton* Skeleton,
                                                              UObject* InParent, FName Name, EObjectFlags Flags)
{
	if (!Skeleton)
	{
		UE_LOG(LogCast, Error, TEXT("Cannot import animation %s: Skeleton is null."), *Name.ToString());
		return nullptr;
	}
	if (AnimInfo.Curves.IsEmpty() && AnimInfo.NotificationTracks.IsEmpty())
	{
		UE_LOG(LogCast, Warning, TEXT("Animation %s contains no curve or notify data. Skipping import."),
		       *Name.ToString());
		return nullptr;
	}
	if (!InParent)
	{
		UE_LOG(LogCast, Error, TEXT("ImportAnimation: InParent is null for %s."), *Name.ToString());
		return nullptr;
	}

	FString AnimNameStr = NoIllegalSigns(Name.ToString());

	uint32 NumFrames = CalculateNumberOfFrames(AnimInfo);
	TMap<FString, FInterpolatedBoneCurve> InterpolatedCurves = ExtractAndInterpolateCurves(
		Skeleton, AnimInfo, NumFrames, Options.bDeleteRootNodeAnim);
	TArray<FPreparedBoneTrack> PreparedBoneTracks = PrepareBoneTrackData(
		Skeleton, InterpolatedCurves, Options, NumFrames);
	TArray<FPreparedNotify> PreparedNotifies;
	if (Options.bImportAnimationNotify && !AnimInfo.NotificationTracks.IsEmpty())
	{
		PreparedNotifies = PrepareAnimationNotifies(AnimInfo);
	}

	if (IsInGameThread())
	{
		FScopedSlowTask SlowTask(100, FText::Format(
			                         NSLOCTEXT("CastImporter", "ImportingAnimationGT",
			                                   "Importing Animation {0} (GT)..."),
			                         FText::FromName(Name)));
		SlowTask.MakeDialog();

		FString CurrentThreadParentPackagePath = InParent->GetPathName();

		SlowTask.EnterProgressFrame(5, NSLOCTEXT("CastImporter", "Anim_CreateAsset_GT", "Creating Asset (GT)..."));
		UAnimSequence* AnimSequence = CreateAsset<UAnimSequence>(CurrentThreadParentPackagePath, AnimNameStr,
		                                                         true /*bAllowReplace*/);

		if (!AnimSequence)
		{
			UE_LOG(LogCast, Error, TEXT("ImportAnimation (GT): Failed to create UAnimSequence asset: %s"),
			       *AnimNameStr);
			return nullptr;
		}
		AnimSequence->SetSkeleton(Skeleton);
		AnimSequence->Modify();

		SlowTask.EnterProgressFrame(10, NSLOCTEXT("CastImporter", "Anim_InitController_GT",
		                                          "Initializing Controller (GT)..."));
		IAnimationDataController& Controller = AnimSequence->GetController();
		InitializeAnimationController(Controller, AnimInfo, NumFrames);

		SlowTask.EnterProgressFrame(60, NSLOCTEXT("CastImporter", "Anim_PopulateTracks_GT",
		                                          "Populating Bone Tracks (GT)..."));
		PopulateBoneTracks(Controller, PreparedBoneTracks);

		SlowTask.EnterProgressFrame(10, NSLOCTEXT("CastImporter", "Anim_AddNotifies_GT", "Adding Notifies (GT)..."));
		if (Options.bImportAnimationNotify && !PreparedNotifies.IsEmpty())
		{
			AddAnimationNotifies(AnimSequence, PreparedNotifies);
		}

		SlowTask.EnterProgressFrame(15, NSLOCTEXT("CastImporter", "Anim_Finalize_GT", "Finalizing (GT)..."));
		FinalizeController(Controller, AnimSequence);

		return AnimSequence;
	}
	TPromise<UAnimSequence*> Promise;
	TFuture<UAnimSequence*> Future = Promise.GetFuture();

	AsyncTask(ENamedThreads::GameThread,
	          [this,
		          LocalAnimInfo = AnimInfo,
		          LocalOptions = Options,
		          LocalSkeleton = Skeleton,
		          LocalInParent = InParent,
		          LocalName = Name,
		          LocalAnimNameStr = AnimNameStr,
		          LocalFlags = Flags,
		          LocalNumFrames = NumFrames,
		          LocalPreparedBoneTracks = MoveTemp(PreparedBoneTracks),
		          LocalPreparedNotifies = MoveTemp(PreparedNotifies),
		          Promise = MoveTemp(Promise)]() mutable
	          {
		          FScopedSlowTask SlowTask(100, FText::Format(
			                                   NSLOCTEXT("CastImporter", "ImportingAnimationGT_Async",
			                                             "Importing Animation {0} (GT Task)..."),
			                                   FText::FromName(LocalName)));
		          SlowTask.MakeDialog();

		          FString GameThreadParentPackagePath = LocalInParent->GetPathName();

		          SlowTask.EnterProgressFrame(5, NSLOCTEXT("CastImporter", "Anim_CreateAsset_GT_Async",
		                                                   "Creating Asset..."));
		          UAnimSequence* AnimSequence = CreateAsset<UAnimSequence>(
			          GameThreadParentPackagePath, LocalAnimNameStr, true);

		          if (!AnimSequence)
		          {
			          UE_LOG(LogCast, Error,
			                 TEXT("ImportAnimation (GT Task): Failed to create UAnimSequence asset: %s"),
			                 *LocalAnimNameStr);
			          Promise.SetValue(nullptr);
			          return;
		          }
		          AnimSequence->SetSkeleton(LocalSkeleton);
		          AnimSequence->Modify();

		          SlowTask.EnterProgressFrame(10, NSLOCTEXT("CastImporter", "Anim_InitController_GT_Async",
		                                                    "Initializing Controller..."));
		          IAnimationDataController& Controller = AnimSequence->GetController();
		          InitializeAnimationController(Controller, LocalAnimInfo, LocalNumFrames);

		          SlowTask.EnterProgressFrame(60, NSLOCTEXT("CastImporter", "Anim_PopulateTracks_GT_Async",
		                                                    "Populating Bone Tracks..."));
		          PopulateBoneTracks(Controller, LocalPreparedBoneTracks);

		          SlowTask.EnterProgressFrame(10, NSLOCTEXT("CastImporter", "Anim_AddNotifies_GT_Async",
		                                                    "Adding Notifies..."));
		          if (LocalOptions.bImportAnimationNotify && !LocalPreparedNotifies.IsEmpty())
		          {
			          AddAnimationNotifies(AnimSequence, LocalPreparedNotifies);
		          }

		          SlowTask.EnterProgressFrame(15, NSLOCTEXT("CastImporter", "Anim_Finalize_GT_Async", "Finalizing..."));
		          FinalizeController(Controller, AnimSequence);

		          Promise.SetValue(AnimSequence);
	          });

	return Future.Get();
}

void FDefaultCastAnimationImporter::FInterpolatedBoneCurve::PadToLength(uint32 Length, FVector3f DefaultPos,
                                                                        FQuat4f DefaultRot, FVector3f DefaultScale)
{
	auto PadFloat = [Length](TArray<float>& Arr, float Default)
	{
		if (Arr.IsEmpty()) Arr.Init(Default, Length);
		else if (Arr.Num() < (int32)Length) Arr.SetNumZeroed(Length);
	};
	auto PadQuat = [Length](TArray<FQuat4f>& Arr, FQuat4f Default)
	{
		if (Arr.IsEmpty()) Arr.Init(Default, Length);
		else if (Arr.Num() < (int32)Length) Arr.SetNumZeroed(Length);
	};

	PadFloat(PositionX, DefaultPos.X);
	PadFloat(PositionY, DefaultPos.Y);
	PadFloat(PositionZ, DefaultPos.Z);
	PadQuat(Rotation, DefaultRot);
	PadFloat(ScaleX, DefaultScale.X);
	PadFloat(ScaleY, DefaultScale.Y);
	PadFloat(ScaleZ, DefaultScale.Z);
}

TArray<FDefaultCastAnimationImporter::FPreparedBoneTrack> FDefaultCastAnimationImporter::PrepareBoneTrackData(
	const USkeleton* Skeleton, const TMap<FString, FInterpolatedBoneCurve>& BoneCurves,
	const FCastImportOptions& Options, uint32 NumberOfFrames) const
{
	TArray<FPreparedBoneTrack> PreparedTracks;
	if (!Skeleton) return PreparedTracks;

	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
	for (const auto& Pair : BoneCurves)
	{
		const FString& BoneNameStr = Pair.Key;
		const FInterpolatedBoneCurve& CurveData = Pair.Value;
		FName BoneName = FName(*BoneNameStr);
		int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName);
		if (BoneIndex == INDEX_NONE) continue;

		FTransform RefPoseTransform = RefSkeleton.GetRefBonePose()[BoneIndex];
		FVector3f DefaultPos = (FVector3f)RefPoseTransform.GetLocation();
		FQuat4f DefaultRot = (FQuat4f)RefPoseTransform.GetRotation();
		FVector3f DefaultScale = (FVector3f)RefPoseTransform.GetScale3D();

		FInterpolatedBoneCurve MutableCurveData = CurveData;
		MutableCurveData.PadToLength(NumberOfFrames, DefaultPos, DefaultRot, DefaultScale);

		FPreparedBoneTrack PreparedTrack;
		PreparedTrack.BoneName = BoneName;
		PreparedTrack.Positions.SetNumUninitialized(NumberOfFrames);
		PreparedTrack.Rotations.SetNumUninitialized(NumberOfFrames);
		PreparedTrack.Scales.SetNumUninitialized(NumberOfFrames);

		for (uint32 Frame = 0; Frame < NumberOfFrames; ++Frame)
		{
			FVector3f FramePos(MutableCurveData.PositionX[Frame], -MutableCurveData.PositionY[Frame],
			                   MutableCurveData.PositionZ[Frame]);
			if (Options.bConvertRefPosition) FramePos += DefaultPos;
			PreparedTrack.Positions[Frame] = FramePos;

			FQuat4f FrameRot = MutableCurveData.Rotation[Frame];
			if (Options.bConvertRefAnim) FrameRot = DefaultRot * FrameRot;
			PreparedTrack.Rotations[Frame] = FrameRot.GetNormalized();

			FVector3f FrameScale(MutableCurveData.ScaleX[Frame], MutableCurveData.ScaleY[Frame],
			                     MutableCurveData.ScaleZ[Frame]);
			PreparedTrack.Scales[Frame] = FrameScale;
		}
		PreparedTracks.Add(MoveTemp(PreparedTrack));
	}
	return PreparedTracks;
}

TArray<FDefaultCastAnimationImporter::FPreparedNotify> FDefaultCastAnimationImporter::PrepareAnimationNotifies(
	const FCastAnimationInfo& Animation) const
{
	TArray<FPreparedNotify> AllPreparedNotifies;
	float FrameRate = FMath::Max(1.0f, Animation.Framerate);
	for (const FCastNotificationTrackInfo& NotifyTrack : Animation.NotificationTracks)
	{
		if (NotifyTrack.Name.IsEmpty() || NotifyTrack.KeyFrameBuffer.IsEmpty()) continue;
		FName NotifyNameFName = FName(*NotifyTrack.Name);
		for (uint32 Frame : NotifyTrack.KeyFrameBuffer)
		{
			FPreparedNotify Prepared;
			Prepared.NotifyName = NotifyNameFName;
			Prepared.Time = (float)Frame / FrameRate;
			AllPreparedNotifies.Add(Prepared);
		}
	}
	return AllPreparedNotifies;
}

void FDefaultCastAnimationImporter::InitializeAnimationController(IAnimationDataController& Controller,
                                                                  const FCastAnimationInfo& Animation,
                                                                  uint32 NumberOfFrames)
{
	Controller.InitializeModel();
	Controller.OpenBracket(NSLOCTEXT("CastImporter", "ImportAnimBracket", "Importing Animation Data"));

	Controller.SetFrameRate(FFrameRate(FMath::Max(1.0f, Animation.Framerate), 1), false); // Ensure valid framerate
	Controller.SetNumberOfFrames(FFrameNumber((int32)NumberOfFrames), false);

	Controller.RemoveAllBoneTracks(false);
}

uint32 FDefaultCastAnimationImporter::CalculateNumberOfFrames(const FCastAnimationInfo& Animation)
{
	uint32 MaxFrame = 0;
	for (const FCastCurveInfo& Curve : Animation.Curves)
	{
		if (!Curve.KeyFrameBuffer.IsEmpty())
		{
			MaxFrame = FMath::Max(MaxFrame, Curve.KeyFrameBuffer.Last() + 1);
		}
	}
	return MaxFrame == 0 ? 1u : MaxFrame;
}

TMap<FString, FDefaultCastAnimationImporter::FInterpolatedBoneCurve> FDefaultCastAnimationImporter::
ExtractAndInterpolateCurves(const USkeleton* Skeleton, const FCastAnimationInfo& Animation, uint32 NumberOfFrames,
                            bool bSkipRootAnim)
{
	TMap<FString, FInterpolatedBoneCurve> BoneCurves;
	FName RootBoneName = Skeleton->GetReferenceSkeleton().GetBoneName(0);

	for (const FCastCurveInfo& Curve : Animation.Curves)
	{
		if (bSkipRootAnim && Curve.NodeName == RootBoneName.ToString()) continue;
		if (Curve.NodeName.IsEmpty() || Curve.KeyPropertyName.IsEmpty()) continue;

		FInterpolatedBoneCurve& BoneCurve = BoneCurves.FindOrAdd(Curve.NodeName);
		if (Curve.Mode != TEXT("absolute")) BoneCurve.AnimMode = ECastAnimImportType::CastAIT_Relative;

		if (Curve.KeyPropertyName == TEXT("tx")) InterpolateCurve(BoneCurve.PositionX, Curve, NumberOfFrames);
		else if (Curve.KeyPropertyName == TEXT("ty")) InterpolateCurve(BoneCurve.PositionY, Curve, NumberOfFrames);
		else if (Curve.KeyPropertyName == TEXT("tz")) InterpolateCurve(BoneCurve.PositionZ, Curve, NumberOfFrames);
		else if (Curve.KeyPropertyName == TEXT("rq")) InterpolateCurve(BoneCurve.Rotation, Curve, NumberOfFrames);
		else if (Curve.KeyPropertyName == TEXT("sx")) InterpolateCurve(BoneCurve.ScaleX, Curve, NumberOfFrames);
		else if (Curve.KeyPropertyName == TEXT("sy")) InterpolateCurve(BoneCurve.ScaleY, Curve, NumberOfFrames);
		else if (Curve.KeyPropertyName == TEXT("sz")) InterpolateCurve(BoneCurve.ScaleZ, Curve, NumberOfFrames);
	}
	return BoneCurves;
}

void FDefaultCastAnimationImporter::PopulateBoneTracks(IAnimationDataController& Controller,
                                                       const TArray<FPreparedBoneTrack>& PreparedTracks)
{
	check(IsInGameThread());
	for (const FPreparedBoneTrack& TrackData : PreparedTracks)
	{
		Controller.AddBoneCurve(TrackData.BoneName, false);
		Controller.SetBoneTrackKeys(TrackData.BoneName, TrackData.Positions, TrackData.Rotations, TrackData.Scales,
		                            false);
	}
}

void FDefaultCastAnimationImporter::AddAnimationNotifies(UAnimSequence* DestSeq,
                                                         const TArray<FPreparedNotify>& PreparedNotifies)
{
	check(IsInGameThread());
	if (!DestSeq || PreparedNotifies.IsEmpty()) return;
	DestSeq->Modify();
	for (const FPreparedNotify& Prepared : PreparedNotifies)
	{
		FAnimNotifyEvent NewEvent;
		NewEvent.NotifyName = Prepared.NotifyName;
		NewEvent.SetTime(Prepared.Time);
		DestSeq->Notifies.Add(NewEvent);
	}
}

void FDefaultCastAnimationImporter::FinalizeController(IAnimationDataController& Controller, UAnimSequence* DestSeq)
{
	Controller.NotifyPopulated();
	Controller.CloseBracket();

	DestSeq->PostEditChange();
	DestSeq->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(DestSeq);
	UE_LOG(LogCast, Log, TEXT("Finalized animation import for: %s"), *DestSeq->GetName());
}
