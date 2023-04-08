// Copyright Epic Games, Inc. All Rights Reserved.

#include "VIAnimNotifyState_MotionWarping.h"
#include "GameFramework/Actor.h"
#include "VIMotionWarpingComponent.h"
#include "VIRootMotionModifier.h"
#include "VIRootMotionModifier_AdjustmentBlendWarp.h"

UVIAnimNotifyState_MotionWarping::UVIAnimNotifyState_MotionWarping(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	VIRootMotionModifierConfig = CreateDefaultSubobject<UVIRootMotionModifierConfig_VaultWarp>(TEXT("Vault Warp"));
}

void UVIAnimNotifyState_MotionWarping::AddVIRootMotionModifier_Implementation(UVIMotionWarpingComponent* VIMotionWarpingComp, UAnimSequenceBase* Animation, float StartTime, float EndTime) const
{
	if (VIMotionWarpingComp && VIRootMotionModifierConfig)
	{
		VIRootMotionModifierConfig->AddVIRootMotionModifier(VIMotionWarpingComp, Animation, StartTime, EndTime);
	}
}