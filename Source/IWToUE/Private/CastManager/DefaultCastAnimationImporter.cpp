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

	FScopedSlowTask SlowTask(100, FText::Format(
		                         NSLOCTEXT("CastImporter", "ImportingAnimation", "Importing Animation {0}..."),
		                         FText::FromName(Name)));
	SlowTask.MakeDialog();

	FString AnimName = NoIllegalSigns(Name.ToString());
	FString ParentPackagePath = InParent->GetPathName();

	SlowTask.EnterProgressFrame(5, NSLOCTEXT("CastImporter", "Anim_CreateAsset", "Creating Asset..."));
	UAnimSequence* AnimSequence = CreateAsset<UAnimSequence>(ParentPackagePath, AnimName, true);

	if (!AnimSequence)
	{
		UE_LOG(LogCast, Error, TEXT("Failed to create UAnimSequence asset object: %s"), *AnimName);
		return nullptr;
	}

	AnimSequence->SetSkeleton(Skeleton);
	AnimSequence->Modify();

	SlowTask.EnterProgressFrame(10, NSLOCTEXT("CastImporter", "Anim_InitController", "Initializing Controller..."));
	IAnimationDataController& Controller = AnimSequence->GetController();
	uint32 NumFrames = CalculateNumberOfFrames(AnimInfo);
	InitializeAnimationController(Controller, AnimInfo, NumFrames);

	SlowTask.EnterProgressFrame(40, NSLOCTEXT("CastImporter", "Anim_ExtractCurves",
	                                          "Extracting & Interpolating Curves..."));
	TMap<FString, FInterpolatedBoneCurve> BoneCurves = ExtractAndInterpolateCurves(
		Skeleton, AnimInfo, NumFrames, Options.bDeleteRootNodeAnim);

	SlowTask.EnterProgressFrame(35, NSLOCTEXT("CastImporter", "Anim_PopulateTracks", "Populating Bone Tracks..."));
	PopulateBoneTracks(Controller, BoneCurves, Options, Skeleton, NumFrames);

	SlowTask.EnterProgressFrame(5, NSLOCTEXT("CastImporter", "Anim_AddNotifies", "Adding Notifies..."));
	if (Options.bImportAnimationNotify && !AnimInfo.NotificationTracks.IsEmpty())
	{
		AddAnimationNotifies(AnimSequence, AnimInfo);
	}

	SlowTask.EnterProgressFrame(5, NSLOCTEXT("CastImporter", "Anim_Finalize", "Finalizing..."));
	FinalizeController(Controller, AnimSequence);

	return AnimSequence;
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
			MaxFrame = FMath::Max(MaxFrame, Curve.KeyFrameBuffer.Last() + 1); // Frame count is last frame index + 1
		}
	}
	if (MaxFrame == 0) MaxFrame = 1;
	return MaxFrame;
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
                                                       const TMap<FString, FInterpolatedBoneCurve>& BoneCurves,
                                                       const FCastImportOptions& Options,
                                                       const USkeleton* Skeleton, uint32 NumberOfFrames)
{
	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();

	for (const auto& Pair : BoneCurves)
	{
		const FString& BoneNameStr = Pair.Key;
		const FInterpolatedBoneCurve& CurveData = Pair.Value;
		FName BoneName = FName(*BoneNameStr);

		int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName);
		if (BoneIndex == INDEX_NONE)
		{
			UE_LOG(LogCast, Warning, TEXT("Animated bone '%s' not found in Skeleton '%s'. Skipping track."),
			       *BoneNameStr, *Skeleton->GetName());
			continue;
		}

		// Get reference pose transform
		FTransform RefPoseTransform = RefSkeleton.GetRefBonePose()[BoneIndex];
		FVector3f DefaultPos = (FVector3f)RefPoseTransform.GetLocation();
		FQuat4f DefaultRot = (FQuat4f)RefPoseTransform.GetRotation();
		FVector3f DefaultScale = (FVector3f)RefPoseTransform.GetScale3D();


		// Create temporary arrays to hold final key data for this bone
		TArray<FVector3f> FinalPositions;
		FinalPositions.SetNumUninitialized(NumberOfFrames);
		TArray<FQuat4f> FinalRotations;
		FinalRotations.SetNumUninitialized(NumberOfFrames);
		TArray<FVector3f> FinalScales;
		FinalScales.SetNumUninitialized(NumberOfFrames);

		// Pad interpolated data to full length if some curves were missing
		// This requires modifying the const Value - maybe clone it first?
		FInterpolatedBoneCurve MutableCurveData = CurveData; // Clone
		MutableCurveData.PadToLength(NumberOfFrames, DefaultPos, DefaultRot, DefaultScale);


		// Process frame by frame
		for (uint32 Frame = 0; Frame < NumberOfFrames; ++Frame)
		{
			// --- Position ---
			FVector3f FramePos(MutableCurveData.PositionX[Frame], -MutableCurveData.PositionY[Frame],
			                   MutableCurveData.PositionZ[Frame]); // Apply coord flip
			if (Options.bConvertRefPosition) // Add ref pose position if requested
			{
				FramePos += DefaultPos;
			}
			FinalPositions[Frame] = FramePos;

			// --- Rotation ---
			FQuat4f FrameRot = MutableCurveData.Rotation[Frame]; // Already flipped during interpolation
			if (Options.bConvertRefAnim) // Apply ref pose rotation if requested (Pre-multiply: Ref * Anim)
			{
				FrameRot = DefaultRot * FrameRot;
			}
			FinalRotations[Frame] = FrameRot.GetNormalized();

			// --- Scale ---
			FVector3f FrameScale(MutableCurveData.ScaleX[Frame], MutableCurveData.ScaleY[Frame],
			                     MutableCurveData.ScaleZ[Frame]);
			// Scaling is usually absolute, rarely added to ref pose scale. Check if needed.
			// if (Options.bConvertRefScale) FrameScale *= DefaultScale;
			FinalScales[Frame] = FrameScale;
		}


		// Add bone curve if needed (or assume it's added elsewhere)
		// if (!Controller.IsBoneTrackExisting(BoneName))
		{
			Controller.AddBoneCurve(BoneName, false); // Add if missing
		}

		// Set the keys for the entire track
		Controller.SetBoneTrackKeys(BoneName, FinalPositions, FinalRotations, FinalScales, false);

		UE_LOG(LogCast, Verbose, TEXT("Populated bone track for '%s' with %d frames."), *BoneNameStr, NumberOfFrames);
	}
}

void FDefaultCastAnimationImporter::AddAnimationNotifies(UAnimSequence* DestSeq, const FCastAnimationInfo& Animation)
{
	if (!DestSeq) return;

	DestSeq->Modify(); // Mark for modification
	// Consider clearing existing notifies: DestSeq->Notifies.Empty();

	float FrameRate = FMath::Max(1.0f, Animation.Framerate);

	for (const FCastNotificationTrackInfo& NotifyTrack : Animation.NotificationTracks)
	{
		if (NotifyTrack.Name.IsEmpty() || NotifyTrack.KeyFrameBuffer.IsEmpty()) continue;

		FName NotifyName = FName(*NotifyTrack.Name); // Convert string to FName

		for (uint32 Frame : NotifyTrack.KeyFrameBuffer)
		{
			FAnimNotifyEvent NewEvent;
			NewEvent.NotifyName = NotifyName; // Use the FName
			NewEvent.SetTime((float)Frame / FrameRate);
			// NewEvent.LinkSequence(DestSeq); // Link to sequence
			// Set other properties if needed (TriggerTimeOffset, etc.)

			DestSeq->Notifies.Add(NewEvent);
			UE_LOG(LogCast, Verbose, TEXT("Added notify '%s' at frame %d (time %f)"), *NotifyTrack.Name, Frame,
			       NewEvent.GetTime());
		}
	}

	// Refresh notifies if needed (usually handled by PostEditChange)
	// DestSeq->RefreshCacheData();
}

void FDefaultCastAnimationImporter::FinalizeController(IAnimationDataController& Controller, UAnimSequence* DestSeq)
{
	Controller.NotifyPopulated(); // Signal that data population is complete
	Controller.CloseBracket(); // Close the transaction bracket opened in Initialize

	DestSeq->PostEditChange(); // Trigger updates after modifications
	DestSeq->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(DestSeq); // Ensure registry knows about it
	UE_LOG(LogCast, Log, TEXT("Finalized animation import for: %s"), *DestSeq->GetName());
}
