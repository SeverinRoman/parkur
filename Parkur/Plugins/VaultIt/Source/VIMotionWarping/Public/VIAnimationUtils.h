#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimTypes.h"

/**
 * A collection of useful functions for skeletal mesh animation.
 */
class FVIAnimationUtils
{
public:
	static void ExtractTransformFromTrack(float Time, int32 NumFrames, float SequenceLength, const struct FRawAnimSequenceTrack& RawTrack, EAnimInterpolationType Interpolation, FTransform& OutAtom);

	// Extract specific frame from raw track and place in OutAtom
	VIMOTIONWARPING_API static void ExtractTransformForFrameFromTrack(const FRawAnimSequenceTrack& RawTrack, int32 Frame, FTransform& OutAtom);
};