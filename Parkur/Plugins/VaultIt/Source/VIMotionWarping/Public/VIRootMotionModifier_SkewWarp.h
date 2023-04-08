// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VIRootMotionModifier.h"
#include "VIRootMotionModifier_SkewWarp.generated.h"

USTRUCT()
struct VIMOTIONWARPING_API FVIRootMotionModifier_SkewWarp : public FVIRootMotionModifier_Warp
{
	GENERATED_BODY()

public:

	FVIRootMotionModifier_SkewWarp(){}
	virtual ~FVIRootMotionModifier_SkewWarp() {}

	virtual UScriptStruct* GetScriptStruct() const { return FVIRootMotionModifier_SkewWarp::StaticStruct(); }
	virtual FTransform ProcessVIRootMotion(UVIMotionWarpingComponent& OwnerComp, const FTransform& InVIRootMotion, float DeltaSeconds) override;
};

UCLASS(meta = (DisplayName = "Skew Warp"))
class VIMOTIONWARPING_API UVIRootMotionModifierConfig_SkewWarp : public UVIRootMotionModifierConfig_Warp
{
	GENERATED_BODY()

public:

	UVIRootMotionModifierConfig_SkewWarp(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer) {}

	virtual void AddVIRootMotionModifier(UVIMotionWarpingComponent* VIMotionWarpingComp, const UAnimSequenceBase* Animation, float StartTime, float EndTime) const override
	{
		UVIRootMotionModifierConfig_SkewWarp::AddVIRootMotionModifierSkewWarp(VIMotionWarpingComp, Animation, StartTime, EndTime, SyncPointName, bWarpTranslation, bIgnoreZAxis, bWarpRotation, RotationType, WarpRotationTimeMultiplier);
	}

	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	static void AddVIRootMotionModifierSkewWarp(UVIMotionWarpingComponent* InVIMotionWarpingComp, const UAnimSequenceBase* InAnimation, float InStartTime, float InEndTime, FName InSyncPointName, bool bInWarpTranslation, bool bInIgnoreZAxis, bool bInWarpRotation, EVIMotionWarpRotationType InRotationType, float InWarpRotationTimeMultiplier = 1.f);
};