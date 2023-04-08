// Copyright (c) 2019-2022 Drowning Dragons Limited. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VITypes.h"
#include "VIBlueprintFunctionLibrary.generated.h"

class ACharacter;
class AVICharacterBase;
struct FPredictProjectilePathResult;

/**
 * 
 */
UCLASS(meta = (ScriptName = "VIBlueprintFunctionLibrary"))
class VAULTIT_API UVIBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	static void MessageLogError(const FString& ErrorMsg, bool bOpenLog = true);

	UFUNCTION(BlueprintPure, Category = Vault)
	static bool VaultAnimSetIsValid(const FVIAnimSet& AnimSet) { return AnimSet.IsValid(); }

	UFUNCTION(BlueprintPure, Category = Replication)
	static bool IsRunningOnServer(AActor* Actor)
	{
		if (Actor)
		{
			return (Actor->GetNetMode() == NM_DedicatedServer || Actor->GetNetMode() == NM_ListenServer);
		}
		return false;
	}

	/**
	 * Get Max Jump Height + Max Vault Height
	 * The maximum height a character can achieve from the ground
	 * Assumes gravity, jump velocity, and vault height do not change during runtime and is not in a physics volume or otherwise modified
	 */
	UFUNCTION(BlueprintPure, Category = Pawn)
	static float GetMaxHeightFromGround(ACharacter* Character);

	UFUNCTION(BlueprintPure, Category = Pawn)
	static float GetMaxVaultHeight(APawn* Pawn);

	/**
	 * Get Max Jump Height + Max Vault Height
	 * The maximum height a pawn can achieve from the ground
	 * Assumes gravity, jump velocity, and vault height do not change during runtime and is not in a physics volume or otherwise modified
	 */
	UFUNCTION(BlueprintPure, Category = Pawn)
	static float GetMaxHeightFromGroundForPawn(APawn* Pawn, const float Gravity, const float JumpZVelocity);

	/**
	 * Get Max Jump Height
	 * When added to max vault height we get the maximum height a character can achieve from the ground
	 * Assumes gravity, jump velocity, and vault height do not change during runtime and is not in a physics volume or otherwise modified
	 */
	UFUNCTION(BlueprintPure, Category = Pawn)
	static float GetMaxJumpHeightForCharacter(ACharacter* Character);

	/**
	 * Get Max Jump Height
	 * When added to max vault height we get the maximum height a character can achieve from the ground
	 * Assumes gravity, jump velocity, and vault height do not change during runtime and is not in a physics volume or otherwise modified
	 */
	UFUNCTION(BlueprintPure, Category = Pawn)
	static float GetMaxJumpHeight(const float Gravity, const float JumpZVelocity)
	{
		if (FMath::Abs(Gravity) > KINDA_SMALL_NUMBER)
		{
			return FMath::Square(JumpZVelocity) / (-2.f * Gravity);
		}
		else
		{
			return 0.f;
		}
	}

	/**
	 * @param InActor: Actor to check
	 * @param bWorldUpIsZ: If true does a much cheaper check when using Z as world up axis, if false will use vector
	 * projection for arbitrary orientation
	 * @return True if character is rising, false if falling
	 */
	UFUNCTION(BlueprintPure, Category = Pawn)
	static bool ActorIsAscending(AActor* InActor, bool bWorldUpIsZ);

	UFUNCTION(BlueprintPure, Category = Animation)
	static float ComputeAnimationPlayRateFromDuration(UAnimSequenceBase* Animation, float Duration = 1.f)
	{
		if (Animation && Duration > 0.f)
		{
			return Animation->GetPlayLength() / Duration;
		}
		return 0.f;
	}

	/**
	 * Called by Anim Graph from VIAnimationInterface::SetBoneFBIK to either enable and update FBIK bone data or disable it
	 */
	UFUNCTION(BlueprintCallable, Category = FBIK)
	static void ToggleBoneFBIK(const FName& BoneName, const FVector& NewLocation, bool bEnable, UPARAM(ref) TArray<FVIBoneFBIKData>& Bones)
	{
		if (FVIBoneFBIKData* Bone = GetBoneForFBIK(BoneName, Bones))
		{
			if (bEnable)
			{
				Bone->Enable(NewLocation);
			}
			else
			{
				Bone->Disable();
			}
		}
	}

	/**
	 * Called by Anim Graph to interpolate array of FBIK bone data
	 */
	UFUNCTION(BlueprintCallable, Category = FBIK)
	static void InterpolateFBIK(float DeltaTime, UPARAM(ref) TArray<FVIBoneFBIKData>& Bones)
	{
		for (FVIBoneFBIKData& Bone : Bones)
		{
			Bone.Update(DeltaTime);
		}
	}

	/** Get specific FBIK Bone Data */
	static FVIBoneFBIKData* GetBoneForFBIK(FName BoneName, TArray<FVIBoneFBIKData>& Bones)
	{
		for (FVIBoneFBIKData& Bone : Bones)
		{
			if (Bone.BoneName.IsEqual(BoneName))
			{
				return &Bone;
			}
		}

		return nullptr;
	}

	/** Get specific FBIK Bone Data */
	static const FVIBoneFBIKData* GetBoneForFBIK(FName BoneName, const TArray<FVIBoneFBIKData>& Bones)
	{
		for (const FVIBoneFBIKData& Bone : Bones)
		{
			if (Bone.BoneName.IsEqual(BoneName))
			{
				return &Bone;
			}
		}

		return nullptr;
	}

	/**
	 * Get specific FBIK Bone Data
	 * This is not checked whether it exists or not; you can add your own based on whether the resulting BoneName is None or not
	 */
	UFUNCTION(BlueprintPure, Category = FBIK, meta = (DisplayName = "Get Bone For FBIK"))
	static FVIBoneFBIKData K2_GetBoneForFBIK(FName BoneName, const TArray<FVIBoneFBIKData>& Bones)
	{
		for (const FVIBoneFBIKData& Bone : Bones)
		{
			if (Bone.BoneName.IsEqual(BoneName))
			{
				return Bone;
			}
		}

		return FVIBoneFBIKData();
	}

	/** Returns the VaultInfo if it exists */
	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FVIVaultInfo GetVaultInfoFromTargetData(const FGameplayAbilityTargetDataHandle& TargetData);
	
	/** 
	 * Predict where a Character or Pawn will land
	 */
	static bool PredictLandingLocation(FPredictProjectilePathResult& OutPredictResult, ACharacter* ForCharacter, const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes, bool bTraceComplex);

	/** 
	 * Predict where a Character or Pawn will land
	 */
	static bool PredictLandingLocation(FPredictProjectilePathResult& OutPredictResult, AActor* ForActor, const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes, float HalfHeight, float Radius, const FVector& Gravity, bool bTraceComplex);

	/** 
	 * Predict where a Character or Pawn will land
	 */
	static bool PredictLandingLocation(FPredictProjectilePathResult& OutPredictResult, AActor* ForActor, const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes, float HalfHeight, float Radius, float GravityZ, bool bTraceComplex);

	/**
	* Predict the arc of a virtual capsule (character) affected by gravity with collision checks along the arc.
	* Returns true if it hit something.
	*
	* @param PredictParams				Input params to the trace (start location, velocity, time to simulate, etc).
	* @param PredictResult				Output result of the trace (Hit result, array of location/velocity/times for each trace step, etc).
	* @return							True if hit something along the path (if tracing with collision).
	*/
	static bool PredictCapsulePath(const UObject* WorldContextObject, float HalfHeight, const struct FPredictProjectilePathParams& PredictParams, struct FPredictProjectilePathResult& PredictResult, const FVector& UpVector);

	/**
	 * Always check CanVault() or similar functionality to ensure character is in a state where they're allowed to vault
	 */
	static FVIVaultResult ComputeVault(APawn* const Pawn, const FVector& InVaultDirection, const FVITraceSettings& TraceSettings, const FVICapsuleInfo& Capsule, bool bTraceComplex);

	/**
	 * Same as engine version but allows for rotation via quaternion
	 * Sweeps a capsule along the given line and returns the first hit encountered.
	 * This only finds objects that are of a type specified by ObjectTypes.
	 * 
	 * @param WorldContext	World context
	 * @param Start			Start of line segment.
	 * @param End			End of line segment.
	 * @param Rot			Actor Quat
	 * @param Radius		Radius of the capsule to sweep
	 * @param HalfHeight	Distance from center of capsule to tip of hemisphere endcap.
	 * @param ObjectTypes	Array of Object Types to trace 
	 * @param bTraceComplex	True to test against complex collision, false to test against simplified collision.
	 * @param OutHit		Properties of the trace hit.
	 * @return				True if there was a hit, false otherwise.
	 */
	static void CapsuleTraceSingleForObjects(const UObject* WorldContextObject, const FVector Start, const FVector End, const FQuat Rot, float Radius, float HalfHeight, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, bool bDrawDebug, FHitResult& OutHit, bool bIgnoreSelf, FLinearColor TraceColor = FLinearColor::Red, FLinearColor TraceHitColor = FLinearColor::Green, float DrawTime = 5.0f);

	static void DrawDebugCapsuleTraceSingle(const UWorld* World, const FVector& Start, const FVector& End, const FQuat& Rot, float Radius, float HalfHeight, bool bDrawDebug, bool bHit, const FHitResult& OutHit, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime);

	/**
	 * When not using Z-Up this will convert a vector to a float that is only in the given direction
	 * It is the equivalent of doing Vector.Z (0,0,1) if Dir is the up vector or Vector.X (1,0,0) if Dir is the forward vector
	 */
	static float ComputeDirectionToFloat(const FVector& Vector, const FVector& Dir);
};
