// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimSequence.h"
#include "VIRootMotionModifier.generated.h"

class UVIMotionWarpingComponent;

/** The possible states of a Root Motion Modifier */
UENUM(BlueprintType)
enum class EVIRootMotionModifierState : uint8
{
	/** The modifier is waiting for the animation to hit the warping window */
	Waiting,

	/** The modifier is active and currently affecting the final root motion */
	Active,

	/** The modifier has been marked for removal. Usually because the warping window is done */
	MarkedForRemoval,

	/** The modifier will remain in the list (as long as the window is active) but will not modify the root motion */
	Disabled
};

/** Base struct for Root Motion Modifiers */
USTRUCT()
struct VIMOTIONWARPING_API FVIRootMotionModifier
{
	GENERATED_BODY()

public:

	/** Source of the root motion we are warping */
	UPROPERTY()
	TWeakObjectPtr<const UAnimSequenceBase> Animation = nullptr;

	/** Start time of the warping window */
	UPROPERTY()
	float StartTime = 0.f;

	/** End time of the warping window */
	UPROPERTY()
	float EndTime = 0.f;

	/** Previous playback time of the animation */
	UPROPERTY()
	float PreviousPosition = 0.f;

	/** Current playback time of the animation */
	UPROPERTY()
	float CurrentPosition = 0.f;

	/** Current blend weight of the animation */
	UPROPERTY()
	float Weight = 0.f;

	/** Whether this modifier runs before the extracted root motion is converted to world space or after */
	UPROPERTY()
	bool bInLocalSpace = false;

	/** Current state */
	UPROPERTY()
	EVIRootMotionModifierState State = EVIRootMotionModifierState::Waiting;

	FVIRootMotionModifier(){}
	virtual ~FVIRootMotionModifier() {}

	/** Returns the actual struct used for RTTI */
	virtual UScriptStruct* GetScriptStruct() const PURE_VIRTUAL(FVIRootMotionModifier::GetScriptStruct, return FVIRootMotionModifier::StaticStruct(););

	/** Updates the state of the modifier. Runs before ProcessVIRootMotion */
	virtual void Update(UVIMotionWarpingComponent& OwnerComp);

	/** Performs the actual modification to the motion */
	virtual FTransform ProcessVIRootMotion(UVIMotionWarpingComponent& OwnerComp, const FTransform& InVIRootMotion, float DeltaSeconds) PURE_VIRTUAL(FVIRootMotionModifier::ProcessVIRootMotion, return FTransform::Identity;);
};

/** Blueprint wrapper around the config properties of a root motion modifier */
UCLASS(abstract, BlueprintType, EditInlineNew)
class VIMOTIONWARPING_API UVIRootMotionModifierConfig : public UObject
{
	GENERATED_BODY()

public:
	
	UVIRootMotionModifierConfig(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer) {}

	/** Adds a VIRootMotionModifier of the type this object represents */
	virtual void AddVIRootMotionModifier(UVIMotionWarpingComponent* InVIMotionWarpingComp, const UAnimSequenceBase* Animation, float StartTime, float EndTime) const PURE_VIRTUAL(UVIRootMotionModifierConfig::AddVIRootMotionModifier,);
};

/** Represents a point of alignment in the world */
USTRUCT(BlueprintType, meta = (HasNativeMake = "VIMotionWarping.VIMotionWarpingUtilities.MakeVIMotionWarpingSyncPoint", HasNativeBreak = "VIMotionWarping.VIMotionWarpingUtilities.BreakVIMotionWarpingSyncPoint"))
struct VIMOTIONWARPING_API FVIMotionWarpingSyncPoint
{
	GENERATED_BODY()

	FVIMotionWarpingSyncPoint()
		: Location(FVector::ZeroVector), Rotation(FQuat::Identity) {}
	FVIMotionWarpingSyncPoint(const FVector& InLocation, const FQuat& InRotation)
		: Location(InLocation), Rotation(InRotation) {}
	FVIMotionWarpingSyncPoint(const FVector& InLocation, const FRotator& InRotation)
		: Location(InLocation), Rotation(InRotation.Quaternion()) {}
	FVIMotionWarpingSyncPoint(const FTransform& InTransform)
		: Location(InTransform.GetLocation()), Rotation(InTransform.GetRotation()) {}

	FORCEINLINE const FVector& GetLocation() const { return Location; }
	FORCEINLINE const FQuat& GetRotation() const { return Rotation; }
	FORCEINLINE FRotator Rotator() const { return Rotation.Rotator(); }

	FORCEINLINE bool operator==(const FVIMotionWarpingSyncPoint& Other) const
	{
		return Other.Location.Equals(Location) && Other.Rotation.Equals(Rotation);
	}

	FORCEINLINE bool operator!=(const FVIMotionWarpingSyncPoint& Other) const
	{
		return !Other.Location.Equals(Location) || !Other.Rotation.Equals(Rotation);
	}

protected:

	UPROPERTY()
	FVector Location;

	UPROPERTY()
	FQuat Rotation;
};

// FVIRootMotionModifier_Warp
///////////////////////////////////////////////////////////////

UENUM(BlueprintType)
enum class EVIMotionWarpRotationType : uint8
{
	/** Character rotates to match the rotation of the sync point */
	Default,

	/** Character rotates to face the sync point */
	Facing,
};

USTRUCT()
struct VIMOTIONWARPING_API FVIRootMotionModifier_Warp : public FVIRootMotionModifier
{
	GENERATED_BODY()

public:

	/** Name used to find the sync point for this modifier */
	UPROPERTY()
	FName SyncPointName = NAME_None;

	/** Whether to warp the translation component of the root motion */
	UPROPERTY()
	bool bWarpTranslation = true;

	/** Whether to ignore the Z component of the translation. Z motion will remain untouched */
	UPROPERTY()
	bool bIgnoreZAxis = true;

	/** Whether to warp the rotation component of the root motion */
	UPROPERTY()
	bool bWarpRotation = true;

	/** Whether rotation should be warp to match the rotation of the sync point or to face the sync point */
	UPROPERTY()
	EVIMotionWarpRotationType RotationType = EVIMotionWarpRotationType::Default;

	/** 
	 * Allow to modify how fast the rotation is warped. 
	 * e.g if the window duration is 2sec and this is 0.5, the target rotation will be reached in 1sec instead of 2sec
	 */
	UPROPERTY()
	float WarpRotationTimeMultiplier = 1.f;

	/** Sync Point used by this modifier as target for the warp. Cached during the Update */
	UPROPERTY()
	FVIMotionWarpingSyncPoint CachedSyncPoint;

	FVIRootMotionModifier_Warp(){}
	virtual ~FVIRootMotionModifier_Warp() {}

	//~ Begin FVIRootMotionModifier Interface
	virtual UScriptStruct* GetScriptStruct() const override { return FVIRootMotionModifier_Warp::StaticStruct(); }
	virtual void Update(UVIMotionWarpingComponent& OwnerComp) override;
	virtual FTransform ProcessVIRootMotion(UVIMotionWarpingComponent& OwnerComp, const FTransform& InVIRootMotion, float DeltaSeconds) override;
	//~ End FVIRootMotionModifier Interface

	/** Event called during update if the sync point changes while the warping is active */
	virtual void OnSyncPointChanged(UVIMotionWarpingComponent& OwnerComp) {}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	void PrintLog(const UVIMotionWarpingComponent& OwnerComp, const FString& Name, const FTransform& OriginalVIRootMotion, const FTransform& WarpedVIRootMotion) const;
#endif

protected:

	FQuat GetTargetRotation(UVIMotionWarpingComponent& OwnerComp) const;
	FQuat WarpRotation(UVIMotionWarpingComponent& OwnerComp, const FTransform& VIRootMotionDelta, const FTransform& VIRootMotionTotal, float DeltaSeconds);
};

UCLASS(meta = (DisplayName = "Simple Warp"))
class VIMOTIONWARPING_API UVIRootMotionModifierConfig_Warp : public UVIRootMotionModifierConfig
{
	GENERATED_BODY()

public:

	/** Name used to find the sync point for this modifier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	FName SyncPointName = NAME_None;

	/** Whether to warp the translation component of the root motion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	bool bWarpTranslation = true;

	/** Whether to ignore the Z component of the translation. Z motion will remain untouched */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (EditCondition = "bWarpTranslation"))
	bool bIgnoreZAxis = true;

	/** Whether to warp the rotation component of the root motion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	bool bWarpRotation = true;

	/** Whether rotation should be warp to match the rotation of the sync point or to face the sync point */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (EditCondition = "bWarpRotation"))
	EVIMotionWarpRotationType RotationType;

	/**
	 * Allow to modify how fast the rotation is warped.
	 * e.g if the window duration is 2sec and this is 0.5, the target rotation will be reached in 1sec instead of 2sec
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (EditCondition = "bWarpRotation"))
	float WarpRotationTimeMultiplier = 1.f;

	UVIRootMotionModifierConfig_Warp(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}

	virtual void AddVIRootMotionModifier(UVIMotionWarpingComponent* VIMotionWarpingComp, const UAnimSequenceBase* Animation, float StartTime, float EndTime) const override
	{
		UVIRootMotionModifierConfig_Warp::AddVIRootMotionModifierSimpleWarp(VIMotionWarpingComp, Animation, StartTime, EndTime, SyncPointName, bWarpTranslation, bIgnoreZAxis, bWarpRotation, RotationType, WarpRotationTimeMultiplier);
	}

	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	static void AddVIRootMotionModifierSimpleWarp(UVIMotionWarpingComponent* InVIMotionWarpingComp, const UAnimSequenceBase* InAnimation, float InStartTime, float InEndTime, FName InSyncPointName, bool bInWarpTranslation, bool bInIgnoreZAxis, bool bInWarpRotation, EVIMotionWarpRotationType InRotationType, float InWarpRotationTimeMultiplier = 1.f);
};

UCLASS(meta = (DisplayName = "Vault Warp"))
class VIMOTIONWARPING_API UVIRootMotionModifierConfig_VaultWarp : public UVIRootMotionModifierConfig_Warp
{
	GENERATED_BODY()

public:
	UVIRootMotionModifierConfig_VaultWarp(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
		SyncPointName = TEXT("VaultSyncPoint");
		bIgnoreZAxis = false;
		RotationType = EVIMotionWarpRotationType::Default;
	}

	virtual void AddVIRootMotionModifier(UVIMotionWarpingComponent* VIMotionWarpingComp, const UAnimSequenceBase* Animation, float StartTime, float EndTime) const override
	{
		UVIRootMotionModifierConfig_Warp::AddVIRootMotionModifierSimpleWarp(VIMotionWarpingComp, Animation, StartTime, EndTime, SyncPointName, bWarpTranslation, bIgnoreZAxis, bWarpRotation, RotationType, WarpRotationTimeMultiplier);
	}
};


// FVIRootMotionModifier_Scale
///////////////////////////////////////////////////////////////

USTRUCT()
struct VIMOTIONWARPING_API FVIRootMotionModifier_Scale : public FVIRootMotionModifier
{
	GENERATED_BODY()

public:

	/** Vector used to scale each component of the translation */
	UPROPERTY()
	FVector Scale = FVector(1.f);

	FVIRootMotionModifier_Scale() { bInLocalSpace = true; }
	virtual ~FVIRootMotionModifier_Scale() {}

	virtual UScriptStruct* GetScriptStruct() const override { return FVIRootMotionModifier_Scale::StaticStruct(); }
	virtual FTransform ProcessVIRootMotion(UVIMotionWarpingComponent& OwnerComp, const FTransform& InVIRootMotion, float DeltaSeconds) override
	{
		FTransform FinalVIRootMotion = InVIRootMotion;
		FinalVIRootMotion.ScaleTranslation(Scale);
		return FinalVIRootMotion;
	}
};

UCLASS(meta = (DisplayName = "Scale"))
class VIMOTIONWARPING_API UVIRootMotionModifierConfig_Scale : public UVIRootMotionModifierConfig
{
	GENERATED_BODY()

public:

	/** Vector used to scale each component of the translation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	FVector Scale = FVector(1.f);

	UVIRootMotionModifierConfig_Scale(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}

	virtual void AddVIRootMotionModifier(UVIMotionWarpingComponent* VIMotionWarpingComp, const UAnimSequenceBase* Animation, float StartTime, float EndTime) const override
	{
		UVIRootMotionModifierConfig_Scale::AddVIRootMotionModifierScale(VIMotionWarpingComp, Animation, StartTime, EndTime, Scale);
	}

	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	static void AddVIRootMotionModifierScale(UVIMotionWarpingComponent* InVIMotionWarpingComp, const UAnimSequenceBase* InAnimation, float InStartTime, float InEndTime, FVector InScale);
};