// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "VIRootMotionModifier.h"
#include "VIMotionWarpingComponent.generated.h"

class ACharacter;
class UAnimSequenceBase;
class UCharacterMovementComponent;
class UVIMotionWarpingComponent;
class UVIAnimNotifyState_MotionWarping;

DECLARE_LOG_CATEGORY_EXTERN(LogVIMotionWarping, Log, All);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
struct FVIMotionWarpingCVars
{
	static TAutoConsoleVariable<int32> CVarVIMotionWarpingDisable;
	static TAutoConsoleVariable<int32> CVarVIMotionWarpingDebug;
	static TAutoConsoleVariable<float> CVarVIMotionWarpingDrawDebugDuration;
};
#endif

USTRUCT(BlueprintType)
struct FVIMotionWarpingWindowData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defaults")
	UVIAnimNotifyState_MotionWarping* VIAnimNotify = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defaults")
	float StartTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defaults")
	float EndTime = 0.f;
};

UCLASS()
class VIMOTIONWARPING_API UVIMotionWarpingUtilities : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Extracts data from a VIMotionWarpingSyncPoint */
	UFUNCTION(BlueprintPure, Category = "Motion Warping", meta = (NativeBreakFunc))
	static void BreakVIMotionWarpingSyncPoint(const FVIMotionWarpingSyncPoint& SyncPoint, FVector& Location, FRotator& Rotation);

	/** Create a VIMotionWarpingSyncPoint struct */
	UFUNCTION(BlueprintPure, Category = "Motion Warping", meta = (NativeMakeFunc))
	static FVIMotionWarpingSyncPoint MakeVIMotionWarpingSyncPoint(FVector Location, FRotator Rotation);

	/** Extract bone pose in local space for all bones in BoneContainer. If Animation is a Montage the pose is extracted from the first track */
	static void ExtractLocalSpacePose(const UAnimSequenceBase* Animation, const FBoneContainer& BoneContainer, float Time, bool bExtractRootMotion, FCompactPose& OutPose);

	/** Extract bone pose in component space for all bones in BoneContainer. If Animation is a Montage the pose is extracted from the first track */
	static void ExtractComponentSpacePose(const UAnimSequenceBase* Animation, const FBoneContainer& BoneContainer, float Time, bool bExtractRootMotion, FCSPose<FCompactPose>& OutPose);

	/** Extract Root Motion transform from a contiguous position range */
	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	static FTransform ExtractVIRootMotionFromAnimation(const UAnimSequenceBase* Animation, float StartTime, float EndTime);

	/** @return All the VIMotionWarping windows within the supplied animation */
	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	static void GetVIMotionWarpingWindowsFromAnimation(const UAnimSequenceBase* Animation, TArray<FVIMotionWarpingWindowData>& OutWindows);

	/** @return All the VIMotionWarping windows within the supplied animation for a given Sync Point */
	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	static void GetVIMotionWarpingWindowsForSyncPointFromAnimation(const UAnimSequenceBase* Animation, FName SyncPointName, TArray<FVIMotionWarpingWindowData>& OutWindows);
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVIMotionWarpingPreUpdate, class UVIMotionWarpingComponent*, VIMotionWarpingComp);

UCLASS(ClassGroup = Movement, meta = (BlueprintSpawnableComponent))
class VIMOTIONWARPING_API UVIMotionWarpingComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	/** Whether to look inside animations within montage when looking for warping windows */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	bool bSearchForWindowsInAnimsWithinMontages;

	/** Event called before Root Motion Modifiers are updated */
	UPROPERTY(BlueprintAssignable, Category = "Motion Warping")
	FVIMotionWarpingPreUpdate OnPreUpdate;

	UVIMotionWarpingComponent(const FObjectInitializer& ObjectInitializer);

	virtual void InitializeComponent() override;

	/** Gets the character this component belongs to */
	FORCEINLINE ACharacter* GetCharacterOwner() const { return CharacterOwner.Get(); }

	/** Returns the list of root motion modifiers */
	FORCEINLINE const TArray<TSharedPtr<FVIRootMotionModifier>>& GetVIRootMotionModifiers() const { return VIRootMotionModifiers; }

	/** Find the SyncPoint associated with a specified name */
	FORCEINLINE const FVIMotionWarpingSyncPoint* FindSyncPoint(const FName& SyncPointName) const { return SyncPoints.Find(SyncPointName); }

	/** Adds or update the sync point associated with a specified name */
	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	void AddOrUpdateSyncPoint(FName Name, const FVIMotionWarpingSyncPoint& SyncPoint);

	/** Removes sync point associated with the specified key  */
	UFUNCTION(BlueprintCallable, Category = "Motion Warping")
	int32 RemoveSyncPoint(FName Name);

	/** Check if we contain a VIRootMotionModifier for the supplied animation and time range */
	bool ContainsModifier(const UAnimSequenceBase* Animation, float StartTime, float EndTime) const;

	/** Add a new modifier */
	void AddVIRootMotionModifier(TSharedPtr<FVIRootMotionModifier> Modifier);

	/** Mark all the modifiers as Disable */
	void DisableAllVIRootMotionModifiers();

protected:

	/** Character this component belongs to */
	UPROPERTY(Transient)
	TWeakObjectPtr<ACharacter> CharacterOwner;

	/** List of root motion modifiers */
	TArray<TSharedPtr<FVIRootMotionModifier>> VIRootMotionModifiers;

	UPROPERTY(Transient)
	TMap<FName, FVIMotionWarpingSyncPoint> SyncPoints;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	TOptional<FVector> OriginalVIRootMotionAccum;
	TOptional<FVector> WarpedVIRootMotionAccum;
#endif

	void Update();

	FTransform ProcessRootMotionPreConvertToWorld(const FTransform& InVIRootMotion, class UCharacterMovementComponent* CharacterMovementComponent, float DeltaSeconds);
	
	FTransform ProcessRootMotionPostConvertToWorld(const FTransform& InVIRootMotion, class UCharacterMovementComponent* CharacterMovementComponent, float DeltaSeconds);
};