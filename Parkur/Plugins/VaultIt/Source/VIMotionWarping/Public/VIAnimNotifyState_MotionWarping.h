// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "VIAnimNotifyState_MotionWarping.generated.h"

/** AnimNotifyState used to define a motion warping window in the animation */
UCLASS(meta = (DisplayName = "VI Motion Warping"))
class VIMOTIONWARPING_API UVIAnimNotifyState_MotionWarping : public UAnimNotifyState
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Config")
	class UVIRootMotionModifierConfig* VIRootMotionModifierConfig;

	UVIAnimNotifyState_MotionWarping(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintNativeEvent, Category = "Motion Warping")
	void AddVIRootMotionModifier(class UVIMotionWarpingComponent* VIMotionWarpingComp, class UAnimSequenceBase* Animation, float StartTime, float EndTime) const;
};