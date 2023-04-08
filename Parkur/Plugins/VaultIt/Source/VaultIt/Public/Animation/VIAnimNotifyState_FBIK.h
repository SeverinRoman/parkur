// Copyright (c) 2019-2021 Drowning Dragons Limited. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "VIAnimNotifyState_FBIK.generated.h"

class USkeletalMeshComponent;
class UAnimSequenceBase;
class APawn;

UENUM(BlueprintType)
enum class EVIFBIKUpdateType : uint8
{
	FUT_Single						UMETA(DisplayName = "Single Update", ToolTip = "Updates once at the start of the notify, and is disabled at the end of the notify"),
	FUT_Tick						UMETA(DisplayName = "Continuous Update", ToolTip = "Updates on tick since the start of the notify, and is disabled at the end of the notify - be sure to LOD, this can be expensive"),
};

UENUM(BlueprintType)
enum class EVIFBIKUpdateRole : uint8
{
	FUR_All							UMETA(DisplayName = "All", ToolTip = "Applies to all roles"),
	FUR_LocalOnly					UMETA(DisplayName = "Local Player Only", ToolTip = "Will only apply to the local player"),
	FUR_SimulatedOnly				UMETA(DisplayName = "Simulated Only", ToolTip = "Will only apply to simulated players"),
};

UENUM(BlueprintType)
enum class EVIFBIKTraceType : uint8
{
	FTT_Simple						UMETA(DisplayName = "Collision", ToolTip = "Use the object's collision to trace against for bone placement"),
	FTT_Complex						UMETA(DisplayName = "Geometry", ToolTip = "Use the object's geometry to trace against for very accurate bone placement - can be a lot more expensive"),
	FTT_ComplexLocalOnly			UMETA(DisplayName = "Geometry Local Player Only", ToolTip = "Use the object's geometry to trace against for the local player only, otherwise use the object's collision"),
};

/** AnimNotifyState used to setup FBIK for a bone during a window in the animation */
UCLASS(meta = (DisplayName = "VI FBIK"))
class VAULTIT_API UVIAnimNotifyState_FBIK : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	/**
	 * The bone that is being planted onto a surface
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimNotify)
	FName BoneName;

	/**
	 * How far ahead of the bone (BoneName) to trace
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimNotify)
	float TraceLength;

	/**
	 * Radius of sphere trace
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimNotify)
	float TraceRadius;

	/**
	 * Offset along the trace direction to provide width for the hand so it doesn't penetrate the surface
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimNotify)
	float BoneOffset;

	/**
	 * If there is no hit, will disable the FBIK for this bone
	 * Default behaviour simply uses the last successful location
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimNotify)
	bool bDisableIfHitFails;

	/**
	 * What type of collision to trace against
	 * Collision will use the collision mesh on the object which is a lot cheaper
	 * Geometry will use a complex trace which uses the object's geometry which can be expensive
	 * Geometry Local Only will use a complex trace only for the local player which can be a good compromise for accuracy where it matters most and cost-cutting elsewhere
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimNotify)
	EVIFBIKTraceType TraceType;

	/**
	 * Whether to update at the start of the notify and never again or on tick
	 * If updating on tick be extra careful to LOD this (as well as the control rig anim graph nodes) so it doesn't waste resources
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimNotify)
	EVIFBIKUpdateType UpdateType;

	/**
	 * Will skip this many ticks if UpdateType is continuous to save on performance costs
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimNotify, meta = (EditCondition = "UpdateType == EVIFBIKUpdateType::FUT_Tick"))
	uint8 TickSkipAmount;

	/**
	 * Which network roles to apply the FBIK to
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimNotify)
	EVIFBIKUpdateRole ApplyToRoles;

	/**
	 * If true, will draw the trace during PIE
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AnimNotify)
	bool bDebugTraceDuringPIE;

protected:
	/** When notify begins, caches bone offset lateral (the side of) to the owner */
	float LateralOffset;

	uint8 SkippedTicks;

public:
	UVIAnimNotifyState_FBIK()
		: BoneName(TEXT("hand_r"))
		, TraceLength(40.f)
		, TraceRadius(20.f)
		, BoneOffset(10.f)
		, bDisableIfHitFails(false)
		, TraceType(EVIFBIKTraceType::FTT_ComplexLocalOnly)
		, UpdateType(EVIFBIKUpdateType::FUT_Tick)
		, TickSkipAmount(3)
		, ApplyToRoles(EVIFBIKUpdateRole::FUR_All)
		, bDebugTraceDuringPIE(false)
		, LateralOffset(0.f)
		, SkippedTicks(0)
	{
#if WITH_EDITOR
		NotifyColor = FColor::Cyan;
#endif  // WITH_EDITOR
	}

#if WITH_EDITOR
	virtual void OnAnimNotifyCreatedInEditor(FAnimNotifyEvent& ContainingAnimNotifyEvent) override final;
#endif  // WITH_EDITOR

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration) override final;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime) override final;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override final;

protected:
	void UpdateFBIK(USkeletalMeshComponent* MeshComp);

	static APawn* GetPawnOwner(const USkeletalMeshComponent* const MeshComp);

	bool HasValidRole(APawn* const PawnOwner) const;

	static bool IsLocalPlayer(const APawn* const PawnOwner);
};