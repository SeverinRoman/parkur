// Copyright Epic Games, Inc. All Rights Reserved.

#include "VIRootMotionModifier.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimMontage.h"
#include "VIMotionWarpingComponent.h"
#include "DrawDebugHelpers.h"

// FVIRootMotionModifier
///////////////////////////////////////////////////////////////

void FVIRootMotionModifier::Update(UVIMotionWarpingComponent& OwnerComp)
{
	const FAnimMontageInstance* VIRootMotionMontageInstance = OwnerComp.GetCharacterOwner()->GetRootMotionAnimMontageInstance();
	const UAnimMontage* Montage = VIRootMotionMontageInstance ? VIRootMotionMontageInstance->Montage : nullptr;

	// Mark for removal if our animation is not relevant anymore
	if (Montage == nullptr || Montage != Animation)
	{
		UE_LOG(LogVIMotionWarping, Verbose, TEXT("VIMotionWarping: Marking VIRootMotionModifier for removal. Reason: Animation is not valid. Char: %s Current Montage: %s. Window: Animation: %s [%f %f] [%f %f]"),
			*GetNameSafe(OwnerComp.GetCharacterOwner()), *GetNameSafe(Montage), *GetNameSafe(Animation.Get()), StartTime, EndTime, PreviousPosition, CurrentPosition);

		State = EVIRootMotionModifierState::MarkedForRemoval;
		return;
	}

	// Update playback times and weight
	PreviousPosition = VIRootMotionMontageInstance->GetPreviousPosition();
	CurrentPosition = VIRootMotionMontageInstance->GetPosition();
	Weight = VIRootMotionMontageInstance->GetWeight();

	// Mark for removal if the animation already passed the warping window
	if (PreviousPosition >= EndTime)
	{
		UE_LOG(LogVIMotionWarping, Verbose, TEXT("VIMotionWarping: Marking VIRootMotionModifier for removal. Reason: Window has ended. Char: %s Animation: %s [%f %f] [%f %f]"),
			*GetNameSafe(OwnerComp.GetCharacterOwner()), *GetNameSafe(Animation.Get()), StartTime, EndTime, PreviousPosition, CurrentPosition);

		State = EVIRootMotionModifierState::MarkedForRemoval;
		return;
	}

	// Check if we are inside the warping window
	if (PreviousPosition >= StartTime && PreviousPosition < EndTime)
	{
		// If we were waiting, switch to active
		if (State == EVIRootMotionModifierState::Waiting)
		{
			State = EVIRootMotionModifierState::Active;
		}
	}
}

// FVIRootMotionModifier_Warp
///////////////////////////////////////////////////////////////

void FVIRootMotionModifier_Warp::Update(UVIMotionWarpingComponent& OwnerComp)
{
	// Update playback times and state
	FVIRootMotionModifier::Update(OwnerComp);

	// Cache sync point transform and trigger OnSyncPointChanged if needed
	if (State == EVIRootMotionModifierState::Active)
	{
		const FVIMotionWarpingSyncPoint* SyncPointPtr = OwnerComp.FindSyncPoint(SyncPointName);

		// Disable if there is no sync point for us
		if (SyncPointPtr == nullptr)
		{
			UE_LOG(LogVIMotionWarping, Verbose, TEXT("VIMotionWarping: Marking VIRootMotionModifier as Disabled. Reason: Invalid Sync Point (%s). Char: %s Animation: %s [%f %f] [%f %f]"),
				*SyncPointName.ToString(), *GetNameSafe(OwnerComp.GetCharacterOwner()), *GetNameSafe(Animation.Get()), StartTime, EndTime, PreviousPosition, CurrentPosition);

			State = EVIRootMotionModifierState::Disabled;
			return;
		}

		if (CachedSyncPoint != *SyncPointPtr)
		{
			CachedSyncPoint = *SyncPointPtr;

			OnSyncPointChanged(OwnerComp);
		}
	}
}

float ComputeDirectionForVector(const FVector& Vector, const FVector& Dir)
{
	const FVector VectorProject = Vector.ProjectOnTo(Dir);
	const float SignDirection = FMath::Sign(VectorProject | Dir);

	return VectorProject.Size() * SignDirection;
}

FTransform FVIRootMotionModifier_Warp::ProcessVIRootMotion(UVIMotionWarpingComponent& OwnerComp, const FTransform& InVIRootMotion, float DeltaSeconds)
{
	const ACharacter* CharacterOwner = OwnerComp.GetCharacterOwner();
	check(CharacterOwner);

	const FTransform& CharacterTransform = CharacterOwner->GetActorTransform();

	FTransform FinalVIRootMotion = InVIRootMotion;

	const FTransform VIRootMotionTotal = UVIMotionWarpingUtilities::ExtractVIRootMotionFromAnimation(Animation.Get(), PreviousPosition, EndTime);

	if (bWarpTranslation)
	{
		const FVector& Up = CharacterOwner->GetActorUpVector();

		FVector DeltaTranslation = InVIRootMotion.GetTranslation();

		const FTransform VIRootMotionDelta = UVIMotionWarpingUtilities::ExtractVIRootMotionFromAnimation(Animation.Get(), PreviousPosition, FMath::Min(CurrentPosition, EndTime));

		const FVector VIRootMotionFwdDelta = FVector::VectorPlaneProject(VIRootMotionDelta.GetTranslation(), Up);
		const FVector CharacterTransformFwd = FVector::VectorPlaneProject(CharacterTransform.GetLocation(), Up);
		const FVector CachedSyncPointFwd = FVector::VectorPlaneProject(CachedSyncPoint.GetLocation(), Up);
		const FVector VIRootMotionTotalFwd = FVector::VectorPlaneProject(VIRootMotionTotal.GetTranslation(), Up);

		const float HorizontalDelta = VIRootMotionFwdDelta.Size();
		const float HorizontalTarget = FVector::Dist(CharacterTransformFwd, CachedSyncPointFwd);
		const float HorizontalOriginal = VIRootMotionTotalFwd.Size();
		const float HorizontalTranslationWarped = HorizontalOriginal != 0.f ? ((HorizontalDelta * HorizontalTarget) / HorizontalOriginal) : 0.f;

		// Original Z-Bound Code
		/*
		const float HorizontalDelta = VIRootMotionDelta.GetTranslation().Size2D();
		const float HorizontalTarget = FVector::Dist2D(CharacterTransform.GetLocation(), CachedSyncPoint.GetLocation());
		const float HorizontalOriginal = VIRootMotionTotal.GetTranslation().Size2D();
		const float HorizontalTranslationWarped = HorizontalOriginal != 0.f ? ((HorizontalDelta * HorizontalTarget) / HorizontalOriginal) : 0.f;
		*/

		if (bInLocalSpace)
		{
			const FTransform MeshRelativeTransform = FTransform(CharacterOwner->GetBaseRotationOffset(), CharacterOwner->GetBaseTranslationOffset());
			const FTransform MeshTransform = MeshRelativeTransform * CharacterOwner->GetActorTransform();
			DeltaTranslation = MeshTransform.InverseTransformPositionNoScale(CachedSyncPoint.GetLocation()).GetSafeNormal2D() * HorizontalTranslationWarped;
		}
		else
		{
			// This is the default (in world space)

			// Custom Arbitrary Up Vector code
			DeltaTranslation = (CachedSyncPointFwd - CharacterTransformFwd).GetSafeNormal() * HorizontalTranslationWarped;

			// Original Z-Bound Code
			//DeltaTranslation = (CachedSyncPoint.GetLocation() - CharacterTransform.GetLocation()).GetSafeNormal2D() * HorizontalTranslationWarped;
		}

		if (!bIgnoreZAxis)
		{
			// Custom Arbitrary Up Vector code
			const FVector CapsuleBottomLocation = (CharacterOwner->GetActorLocation() - (Up * CharacterOwner->GetSimpleCollisionHalfHeight()));
			const float VerticalDelta = ComputeDirectionForVector(VIRootMotionDelta.GetTranslation(), Up);
			const float VerticalTarget = ComputeDirectionForVector(CachedSyncPoint.GetLocation(), Up) - ComputeDirectionForVector(CapsuleBottomLocation, Up);
			const float VerticalOriginal = ComputeDirectionForVector(VIRootMotionTotal.GetTranslation(), Up);
			const float VerticalTranslationWarped = VerticalOriginal != 0.f ? ((VerticalDelta * VerticalTarget) / VerticalOriginal) : 0.f;

			// Remove the Z component
			DeltaTranslation = FVector::VectorPlaneProject(DeltaTranslation, Up);

			// Add in new Z component
			DeltaTranslation += Up * VerticalTranslationWarped;

			// Original Z-Bound Code
			/*
			const FVector CapsuleBottomLocation = (CharacterOwner->GetActorLocation() - FVector::UpVector * CharacterOwner->GetSimpleCollisionHalfHeight());
			const float VerticalDelta = VIRootMotionDelta.GetTranslation().Z;
			const float VerticalTarget = CachedSyncPoint.GetLocation().Z - CapsuleBottomLocation.Z;
			const float VerticalOriginal = VIRootMotionTotal.GetTranslation().Z;
			const float VerticalTranslationWarped = VerticalOriginal != 0.f ? ((VerticalDelta * VerticalTarget) / VerticalOriginal) : 0.f;

			DeltaTranslation.Z = VerticalTranslationWarped;
			*/
		}

		FinalVIRootMotion.SetTranslation(DeltaTranslation);
	}

	if (bWarpRotation)
	{
		const FQuat WarpedRotation = WarpRotation(OwnerComp, InVIRootMotion, VIRootMotionTotal, DeltaSeconds);
		FinalVIRootMotion.SetRotation(WarpedRotation);
	}

	// Debug
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const int32 DebugLevel = FVIMotionWarpingCVars::CVarVIMotionWarpingDebug.GetValueOnGameThread();
	if (DebugLevel > 0)
	{
		PrintLog(OwnerComp, TEXT("FVIRootMotionModifier_Simple"), InVIRootMotion, FinalVIRootMotion);

		if (DebugLevel >= 2)
		{
			const float DrawDebugDuration = FVIMotionWarpingCVars::CVarVIMotionWarpingDrawDebugDuration.GetValueOnGameThread();
			DrawDebugCoordinateSystem(OwnerComp.GetWorld(), CachedSyncPoint.GetLocation(), CachedSyncPoint.Rotator(), 50.f, false, DrawDebugDuration, 0, 1.f);
		}
	}
#endif

	return FinalVIRootMotion;
}

FQuat FVIRootMotionModifier_Warp::GetTargetRotation(UVIMotionWarpingComponent& OwnerComp) const
{
	const FTransform& CharacterTransform = OwnerComp.GetCharacterOwner()->GetActorTransform();

	if (RotationType == EVIMotionWarpRotationType::Default)
	{
		return CachedSyncPoint.GetRotation();
	}
	else if (RotationType == EVIMotionWarpRotationType::Facing)
	{
		const FVector ToSyncPoint = (CachedSyncPoint.GetLocation() - CharacterTransform.GetLocation()).GetSafeNormal2D();
		return FRotationMatrix::MakeFromXZ(ToSyncPoint, FVector::UpVector).ToQuat();
	}

	return FQuat::Identity;
}

FQuat FVIRootMotionModifier_Warp::WarpRotation(UVIMotionWarpingComponent& OwnerComp, const FTransform& VIRootMotionDelta, const FTransform& VIRootMotionTotal, float DeltaSeconds)
{
	const FTransform& CharacterTransform = OwnerComp.GetCharacterOwner()->GetActorTransform();
	const FQuat CurrentRotation = CharacterTransform.GetRotation();
	const FQuat TargetRotation = GetTargetRotation(OwnerComp);
	const float TimeRemaining = (EndTime - PreviousPosition) * WarpRotationTimeMultiplier;
	const FQuat RemainingRootRotationInWorld = VIRootMotionTotal.GetRotation();
	const FQuat CurrentPlusRemainingVIRootMotion = RemainingRootRotationInWorld * CurrentRotation;
	const float PercentThisStep = FMath::Clamp(DeltaSeconds / TimeRemaining, 0.f, 1.f);
	const FQuat TargetRotThisFrame = FQuat::Slerp(CurrentPlusRemainingVIRootMotion, TargetRotation, PercentThisStep);
	const FQuat DeltaOut = TargetRotThisFrame * CurrentPlusRemainingVIRootMotion.Inverse();

	return (DeltaOut * VIRootMotionDelta.GetRotation());
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
void FVIRootMotionModifier_Warp::PrintLog(const UVIMotionWarpingComponent& OwnerComp, const FString& Name, const FTransform& OriginalVIRootMotion, const FTransform& WarpedVIRootMotion) const
{
	const ACharacter* CharacterOwner = OwnerComp.GetCharacterOwner();
	check(CharacterOwner);

	const FVector CurrentLocation = (CharacterOwner->GetActorLocation() - FVector::UpVector * CharacterOwner->GetSimpleCollisionHalfHeight());
	const FVector CurrentToTarget = (CachedSyncPoint.GetLocation() - CurrentLocation).GetSafeNormal2D();
	const FVector FutureLocation = CurrentLocation + (bInLocalSpace ? (CharacterOwner->GetMesh()->ConvertLocalRootMotionToWorld(WarpedVIRootMotion)).GetTranslation() : WarpedVIRootMotion.GetTranslation());
	const FRotator CurrentRotation = CharacterOwner->GetActorRotation();
	const FRotator FutureRotation = (WarpedVIRootMotion.GetRotation() * CharacterOwner->GetActorQuat()).Rotator();
	const float Dot = FVector::DotProduct(CharacterOwner->GetActorForwardVector(), CurrentToTarget);
	const float CurrentDist2D = FVector::Dist2D(CachedSyncPoint.GetLocation(), CurrentLocation);
	const float FutureDist2D = FVector::Dist2D(CachedSyncPoint.GetLocation(), FutureLocation);
	const float DeltaSeconds = CharacterOwner->GetWorld()->GetDeltaSeconds();
	const float Speed = WarpedVIRootMotion.GetTranslation().Size() / DeltaSeconds;
	const float EndTimeOffset = CurrentPosition - EndTime;

	UE_LOG(LogVIMotionWarping, Log, TEXT("VIMotionWarping: %s. NetMode: %d Char: %s Anim: %s Window [%f %f][%f %f] DeltaTime: %f WorldTime: %f EndTimeOffset: %f Dist2D: %f FutureDist2D: %f Dot: %f OriginalMotionDelta: %s (%f) FinalMotionDelta: %s (%f) Speed: %f Loc: %s FutureLoc: %s Rot: %s FutureRot: %s"),
		*Name, (int32)CharacterOwner->GetWorld()->GetNetMode(), *GetNameSafe(CharacterOwner), *GetNameSafe(Animation.Get()), StartTime, EndTime, PreviousPosition, CurrentPosition, DeltaSeconds, CharacterOwner->GetWorld()->GetTimeSeconds(), EndTimeOffset, CurrentDist2D, FutureDist2D, Dot,
		*OriginalVIRootMotion.GetTranslation().ToString(), OriginalVIRootMotion.GetTranslation().Size(), *WarpedVIRootMotion.GetTranslation().ToString(), WarpedVIRootMotion.GetTranslation().Size(), Speed,
		*CurrentLocation.ToString(), *FutureLocation.ToString(), *CurrentRotation.ToCompactString(), *FutureRotation.ToCompactString());
}
#endif

// UVIRootMotionModifierConfig_Warp
///////////////////////////////////////////////////////////////

void UVIRootMotionModifierConfig_Warp::AddVIRootMotionModifierSimpleWarp(UVIMotionWarpingComponent* InVIMotionWarpingComp, const UAnimSequenceBase* InAnimation, float InStartTime, float InEndTime, FName InSyncPointName, bool bInWarpTranslation, bool bInIgnoreZAxis, bool bInWarpRotation, EVIMotionWarpRotationType InRotationType, float InWarpRotationTimeMultiplier)
{
	if (ensureAlways(InVIMotionWarpingComp))
	{
		TSharedPtr<FVIRootMotionModifier_Warp> NewModifier = MakeShared<FVIRootMotionModifier_Warp>();
		NewModifier->Animation = InAnimation;
		NewModifier->StartTime = InStartTime;
		NewModifier->EndTime = InEndTime;
		NewModifier->SyncPointName = InSyncPointName;
		NewModifier->bWarpTranslation = bInWarpTranslation;
		NewModifier->bIgnoreZAxis = bInIgnoreZAxis;
		NewModifier->bWarpRotation = bInWarpRotation;
		NewModifier->RotationType = InRotationType;
		NewModifier->WarpRotationTimeMultiplier = InWarpRotationTimeMultiplier;
		InVIMotionWarpingComp->AddVIRootMotionModifier(NewModifier);
	}
}

// UVIRootMotionModifierConfig_Scale
///////////////////////////////////////////////////////////////

void UVIRootMotionModifierConfig_Scale::AddVIRootMotionModifierScale(UVIMotionWarpingComponent* InVIMotionWarpingComp, const UAnimSequenceBase* InAnimation, float InStartTime, float InEndTime, FVector InScale)
{
	if (ensureAlways(InVIMotionWarpingComp))
	{
		TSharedPtr<FVIRootMotionModifier_Scale> NewModifier = MakeShared<FVIRootMotionModifier_Scale>();
		NewModifier->Animation = InAnimation;
		NewModifier->StartTime = InStartTime;
		NewModifier->EndTime = InEndTime;
		NewModifier->Scale = InScale;
		InVIMotionWarpingComp->AddVIRootMotionModifier(NewModifier);
	}
}