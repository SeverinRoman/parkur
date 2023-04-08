

#include "VIAnimationUtils.h"
#include "AnimationRuntime.h"

void FVIAnimationUtils::ExtractTransformFromTrack(float Time, int32 NumFrames, float SequenceLength, const struct FRawAnimSequenceTrack& RawTrack, EAnimInterpolationType Interpolation, FTransform& OutAtom)
{
	// Bail out (with rather wacky data) if data is empty for some reason.
	if (RawTrack.PosKeys.Num() == 0 || RawTrack.RotKeys.Num() == 0)
	{
		OutAtom.SetIdentity();
		return;
	}

	int32 KeyIndex1, KeyIndex2;
	float Alpha;
	FAnimationRuntime::GetKeyIndicesFromTime(KeyIndex1, KeyIndex2, Alpha, Time, NumFrames, SequenceLength);
	// @Todo fix me: this change is not good, it has lots of branches. But we'd like to save memory for not saving scale if no scale change exists
	
	static const FVector DefaultScale3D = FVector(1.f);

	if (Interpolation == EAnimInterpolationType::Step)
	{
		Alpha = 0.f;
	}

	if (Alpha <= 0.f)
	{
		ExtractTransformForFrameFromTrack(RawTrack, KeyIndex1, OutAtom);
		return;
	}
	else if (Alpha >= 1.f)
	{
		ExtractTransformForFrameFromTrack(RawTrack, KeyIndex2, OutAtom);
		return;
	}

	const int32 PosKeyIndex1 = FMath::Min(KeyIndex1, RawTrack.PosKeys.Num() - 1);
	const int32 RotKeyIndex1 = FMath::Min(KeyIndex1, RawTrack.RotKeys.Num() - 1);

	const int32 PosKeyIndex2 = FMath::Min(KeyIndex2, RawTrack.PosKeys.Num() - 1);
	const int32 RotKeyIndex2 = FMath::Min(KeyIndex2, RawTrack.RotKeys.Num() - 1);

	FTransform KeyAtom1, KeyAtom2;

	const bool bHasScaleKey = (RawTrack.ScaleKeys.Num() > 0);
	if (bHasScaleKey)
	{
		const int32 ScaleKeyIndex1 = FMath::Min(KeyIndex1, RawTrack.ScaleKeys.Num() - 1);
		const int32 ScaleKeyIndex2 = FMath::Min(KeyIndex2, RawTrack.ScaleKeys.Num() - 1);

		KeyAtom1 = FTransform(FQuat(RawTrack.RotKeys[RotKeyIndex1]), FVector(RawTrack.PosKeys[PosKeyIndex1]), FVector(RawTrack.ScaleKeys[ScaleKeyIndex1]));
		KeyAtom2 = FTransform(FQuat(RawTrack.RotKeys[RotKeyIndex2]), FVector(RawTrack.PosKeys[PosKeyIndex2]), FVector(RawTrack.ScaleKeys[ScaleKeyIndex2]));
	}
	else
	{
		KeyAtom1 = FTransform(FQuat(RawTrack.RotKeys[RotKeyIndex1]), FVector(RawTrack.PosKeys[PosKeyIndex1]), DefaultScale3D);
		KeyAtom2 = FTransform(FQuat(RawTrack.RotKeys[RotKeyIndex2]), FVector(RawTrack.PosKeys[PosKeyIndex2]), DefaultScale3D);
	}

	// 	UE_LOG(LogAnimation, Log, TEXT(" *  *  *  Position. PosKeyIndex1: %3d, PosKeyIndex2: %3d, Alpha: %f"), PosKeyIndex1, PosKeyIndex2, Alpha);
	// 	UE_LOG(LogAnimation, Log, TEXT(" *  *  *  Rotation. RotKeyIndex1: %3d, RotKeyIndex2: %3d, Alpha: %f"), RotKeyIndex1, RotKeyIndex2, Alpha);

	// Ensure rotations are normalized (Added for Jira UE-53971)
	KeyAtom1.NormalizeRotation();
	KeyAtom2.NormalizeRotation();

	OutAtom.Blend(KeyAtom1, KeyAtom2, Alpha);
	OutAtom.NormalizeRotation();
}

void FVIAnimationUtils::ExtractTransformForFrameFromTrack(const FRawAnimSequenceTrack& RawTrack, int32 Frame, FTransform& OutAtom)
{
	static const FVector DefaultScale3D = FVector(1.f);

	const int32 PosKeyIndex1 = FMath::Min(Frame, RawTrack.PosKeys.Num() - 1);
	const int32 RotKeyIndex1 = FMath::Min(Frame, RawTrack.RotKeys.Num() - 1);

	if (RawTrack.ScaleKeys.Num() > 0)
	{
		const int32 ScaleKeyIndex1 = FMath::Min(Frame, RawTrack.ScaleKeys.Num() - 1);
		OutAtom = FTransform(FQuat(RawTrack.RotKeys[RotKeyIndex1]), FVector(RawTrack.PosKeys[PosKeyIndex1]), FVector(RawTrack.ScaleKeys[ScaleKeyIndex1]));
	}
	else
	{
		OutAtom = FTransform(FQuat(RawTrack.RotKeys[RotKeyIndex1]), FVector(RawTrack.PosKeys[PosKeyIndex1]), FVector(DefaultScale3D));
	}
}
