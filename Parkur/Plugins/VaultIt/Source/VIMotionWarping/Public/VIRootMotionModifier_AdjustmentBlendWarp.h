// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VIRootMotionModifier.h"
#include "VIRootMotionModifier_AdjustmentBlendWarp.generated.h"

USTRUCT()
struct FVIMotionDeltaTrack
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FTransform> BoneTransformTrack;

	UPROPERTY()
	TArray<FVector> DeltaTranslationTrack;

	UPROPERTY()
	TArray<FRotator> DeltaRotationTrack;

	UPROPERTY()
	FVector TotalTranslation = FVector::ZeroVector;

	UPROPERTY()
	FRotator TotalRotation = FRotator::ZeroRotator;
};

USTRUCT()
struct FVIMotionDeltaTrackContainer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FVIMotionDeltaTrack> Tracks;

	void Init(int32 InNumTracks)
	{
		Tracks.Reserve(InNumTracks);
	}
};

USTRUCT()
struct VIMOTIONWARPING_API FVIRootMotionModifier_AdjustmentBlendWarp : public FVIRootMotionModifier_Warp
{
	GENERATED_BODY()

public:

	UPROPERTY()
	bool bWarpIKBones = false;

	UPROPERTY()
	TArray<FName> IKBones;

	FVIRootMotionModifier_AdjustmentBlendWarp();
	virtual ~FVIRootMotionModifier_AdjustmentBlendWarp() {}

	virtual UScriptStruct* GetScriptStruct() const { return FVIRootMotionModifier_AdjustmentBlendWarp::StaticStruct(); }
	virtual void OnSyncPointChanged(UVIMotionWarpingComponent& OwnerComp) override;
	virtual FTransform ProcessVIRootMotion(UVIMotionWarpingComponent& OwnerComp, const FTransform& InVIRootMotion, float DeltaSeconds) override;

	void GetIKBoneTransformAndAlpha(FName BoneName, FTransform& OutTransform, float& OutAlpha) const;

protected:

	UPROPERTY()
	FTransform CachedMeshTransform;

	UPROPERTY()
	FTransform CachedMeshRelativeTransform;

	UPROPERTY()
	FTransform CachedVIRootMotion;

	UPROPERTY()
	FAnimSequenceTrackContainer Result;

	UPROPERTY()
	float ActualStartTime = 0.f;

	void PrecomputeWarpedTracks(UVIMotionWarpingComponent& OwnerComp);

	FTransform ExtractWarpedVIRootMotion() const;

	void ExtractBoneTransformAtTime(FTransform& OutTransform, const FName& BoneName, float Time) const;
	void ExtractBoneTransformAtTime(FTransform& OutTransform, int32 TrackIndex, float Time) const;
	void ExtractBoneTransformAtFrame(FTransform& OutTransform, int32 TrackIndex, int32 Frame) const;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	void DrawDebugWarpedTracks(UVIMotionWarpingComponent& OwnerComp, float DrawDuration) const;
#endif

	static void ExtractMotionDeltaFromRange(const FBoneContainer& BoneContainer, const UAnimSequenceBase* Animation, float StartTime, float EndTime, float SampleRate, FVIMotionDeltaTrackContainer& OutMotionDeltaTracks);

	static void AdjustmentBlendWarp(const FBoneContainer& BoneContainer, const FCSPose<FCompactPose>& AdditivePose, const FVIMotionDeltaTrackContainer& MotionDeltaTracks, FAnimSequenceTrackContainer& Output);
};

UCLASS(meta = (DisplayName = "Adjustment Blend Warp"))
class VIMOTIONWARPING_API UVIRootMotionModifierConfig_AdjustmentBlendWarp : public UVIRootMotionModifierConfig_Warp
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	bool bWarpIKBones;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (EditCondition = "bWarpIKBones"))
	TArray<FName> IKBones;

	UVIRootMotionModifierConfig_AdjustmentBlendWarp(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer) {}

	virtual void AddVIRootMotionModifier(UVIMotionWarpingComponent* VIMotionWarpingComp, const UAnimSequenceBase* Animation, float StartTime, float EndTime) const override
	{
		UVIRootMotionModifierConfig_AdjustmentBlendWarp::AddVIRootMotionModifierAdjustmentBlendWarp(VIMotionWarpingComp, Animation, StartTime, EndTime, SyncPointName, bWarpTranslation, bIgnoreZAxis, bWarpRotation, bWarpIKBones, IKBones);
	}

	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	static void AddVIRootMotionModifierAdjustmentBlendWarp(UVIMotionWarpingComponent* InVIMotionWarpingComp, const UAnimSequenceBase* InAnimation, float InStartTime, float InEndTime, FName InSyncPointName, bool bInWarpTranslation, bool bInIgnoreZAxis, bool bInWarpRotation, bool bInWarpIKBones, const TArray<FName>& InIKBones);

	UFUNCTION(BlueprintPure, Category = "Motion Warping")
	static void GetIKBoneTransformAndAlpha(ACharacter* Character, FName BoneName, FTransform& OutTransform, float& OutAlpha);
};