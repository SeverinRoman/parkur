// Copyright Epic Games, Inc. All Rights Reserved.

#include "VIRootMotionModifier_SkewWarp.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "VIMotionWarpingComponent.h"

FTransform FVIRootMotionModifier_SkewWarp::ProcessVIRootMotion(UVIMotionWarpingComponent& OwnerComp, const FTransform& InVIRootMotion, float DeltaSeconds)
{
	const ACharacter* CharacterOwner = OwnerComp.GetCharacterOwner(); 
	check(CharacterOwner);

	FTransform FinalVIRootMotion = InVIRootMotion;

	const FTransform VIRootMotionTotal = UVIMotionWarpingUtilities::ExtractVIRootMotionFromAnimation(Animation.Get(), PreviousPosition, EndTime);
	const FTransform VIRootMotionDelta = UVIMotionWarpingUtilities::ExtractVIRootMotionFromAnimation(Animation.Get(), PreviousPosition, FMath::Min(CurrentPosition, EndTime));

	if (bWarpTranslation && !VIRootMotionDelta.GetTranslation().IsNearlyZero())
	{
		const FTransform CurrentTransform = FTransform(
			CharacterOwner->GetActorQuat(),
			CharacterOwner->GetActorLocation() - FVector::UpVector * CharacterOwner->GetSimpleCollisionHalfHeight()
		);

		const FTransform MeshRelativeTransform = FTransform(CharacterOwner->GetBaseRotationOffset(), CharacterOwner->GetBaseTranslationOffset());
		const FTransform MeshTransform = MeshRelativeTransform * CharacterOwner->GetActorTransform();
		const FTransform VIRootMotionTotalWorldSpace = VIRootMotionTotal * MeshTransform;
		const FTransform VIRootMotionDeltaWorldSpace = CharacterOwner->GetMesh()->ConvertLocalRootMotionToWorld(VIRootMotionDelta);

		const FVector CurrentLocation = CurrentTransform.GetLocation();
		const FQuat CurrentRotation = CurrentTransform.GetRotation();
		
		FVector TargetLocation = CachedSyncPoint.GetLocation();
		if(bIgnoreZAxis)
		{
			TargetLocation.Z = CurrentLocation.Z;
		}

		const FVector Translation = VIRootMotionDeltaWorldSpace.GetTranslation();
		const FVector FutureLocation = VIRootMotionTotalWorldSpace.GetLocation();
		const FVector CurrentToWorldOffset = TargetLocation - CurrentLocation;
		const FVector CurrentToRootOffset = FutureLocation - CurrentLocation;

		// Create a matrix we can use to put everything into a space looking straight at VIRootMotionSyncPosition. "forward" should be the axis along which we want to scale. 
		FVector ToRootNormalized = CurrentToRootOffset.GetSafeNormal();

		float BestMatchDot = FMath::Abs(FVector::DotProduct(ToRootNormalized, CurrentRotation.GetAxisX()));
		FMatrix ToRootSyncSpace = FRotationMatrix::MakeFromXZ(ToRootNormalized, CurrentRotation.GetAxisZ());

		float ZDot = FMath::Abs(FVector::DotProduct(ToRootNormalized, CurrentRotation.GetAxisZ()));
		if (ZDot > BestMatchDot)
		{
			ToRootSyncSpace = FRotationMatrix::MakeFromXZ(ToRootNormalized, CurrentRotation.GetAxisX());
			BestMatchDot = ZDot;
		}

		float YDot = FMath::Abs(FVector::DotProduct(ToRootNormalized, CurrentRotation.GetAxisY()));
		if (YDot > BestMatchDot)
		{
			ToRootSyncSpace = FRotationMatrix::MakeFromXZ(ToRootNormalized, CurrentRotation.GetAxisZ());
		}

		// Put everything into RootSyncSpace.
		const FVector VIRootMotionInSyncSpace = ToRootSyncSpace.InverseTransformVector(Translation);
		const FVector CurrentToWorldSync = ToRootSyncSpace.InverseTransformVector(CurrentToWorldOffset);
		const FVector CurrentToVIRootMotionSync = ToRootSyncSpace.InverseTransformVector(CurrentToRootOffset);

		FVector CurrentToWorldSyncNorm = CurrentToWorldSync;
		CurrentToWorldSyncNorm.Normalize();

		FVector CurrentToVIRootMotionSyncNorm = CurrentToVIRootMotionSync;
		CurrentToVIRootMotionSyncNorm.Normalize();

		// Calculate skew Yaw Angle. 
		FVector FlatToWorld = FVector(CurrentToWorldSyncNorm.X, CurrentToWorldSyncNorm.Y, 0.0f);
		FlatToWorld.Normalize();
		FVector FlatToRoot = FVector(CurrentToVIRootMotionSyncNorm.X, CurrentToVIRootMotionSyncNorm.Y, 0.0f);
		FlatToRoot.Normalize();
		float AngleAboutZ = FMath::Acos(FVector::DotProduct(FlatToWorld, FlatToRoot));
		float AngleAboutZNorm = FMath::DegreesToRadians(FRotator::NormalizeAxis(FMath::RadiansToDegrees(AngleAboutZ)));
		if (FlatToWorld.Y < 0.0f)
		{
			AngleAboutZNorm *= -1.0f;
		}

		// Calculate Skew Pitch Angle. 
		FVector ToWorldNoY = FVector(CurrentToWorldSyncNorm.X, 0.0f, CurrentToWorldSyncNorm.Z);
		ToWorldNoY.Normalize();
		FVector ToRootNoY = FVector(CurrentToVIRootMotionSyncNorm.X, 0.0f, CurrentToVIRootMotionSyncNorm.Z);
		ToRootNoY.Normalize();
		const float AngleAboutY = FMath::Acos(FVector::DotProduct(ToWorldNoY, ToRootNoY));
		float AngleAboutYNorm = FMath::DegreesToRadians(FRotator::NormalizeAxis(FMath::RadiansToDegrees(AngleAboutY)));
		if (ToWorldNoY.Z < 0.0f)
		{
			AngleAboutYNorm *= -1.0f;
		}

		FVector SkewedVIRootMotion = FVector::ZeroVector;
		float ProjectedScale = FVector::DotProduct(CurrentToWorldSync, CurrentToVIRootMotionSyncNorm) / CurrentToVIRootMotionSync.Size();
		if (ProjectedScale != 0.0f)
		{
			FMatrix ScaleMatrix;
			ScaleMatrix.SetIdentity();
			ScaleMatrix.SetAxis(0, FVector(ProjectedScale, 0.0f, 0.0f));
			ScaleMatrix.SetAxis(1, FVector(0.0f, 1.0f, 0.0f));
			ScaleMatrix.SetAxis(2, FVector(0.0f, 0.0f, 1.0f));

			FMatrix ShearXAlongYMatrix;
			ShearXAlongYMatrix.SetIdentity();
			ShearXAlongYMatrix.SetAxis(0, FVector(1.0f, FMath::Tan(AngleAboutZNorm), 0.0f));
			ShearXAlongYMatrix.SetAxis(1, FVector(0.0f, 1.0f, 0.0f));
			ShearXAlongYMatrix.SetAxis(2, FVector(0.0f, 0.0f, 1.0f));

			FMatrix ShearXAlongZMatrix;
			ShearXAlongZMatrix.SetIdentity();
			ShearXAlongZMatrix.SetAxis(0, FVector(1.0f, 0.0f, FMath::Tan(AngleAboutYNorm)));
			ShearXAlongZMatrix.SetAxis(1, FVector(0.0f, 1.0f, 0.0f));
			ShearXAlongZMatrix.SetAxis(2, FVector(0.0f, 0.0f, 1.0f));

			FMatrix ScaledSkewMatrix = ScaleMatrix * ShearXAlongYMatrix * ShearXAlongZMatrix;

			// Skew and scale the Root motion. 
			SkewedVIRootMotion = ScaledSkewMatrix.TransformVector(VIRootMotionInSyncSpace);
		}
		else if (!CurrentToVIRootMotionSync.IsZero() && !CurrentToWorldSync.IsZero() && !VIRootMotionInSyncSpace.IsZero())
		{
			// Figure out ratio between remaining Root and remaining World. Then project scaled length of current Root onto World.
			const float Scale = CurrentToWorldSync.Size() / CurrentToVIRootMotionSync.Size();
			const float StepTowardTarget = VIRootMotionInSyncSpace.ProjectOnTo(VIRootMotionInSyncSpace).Size();
			SkewedVIRootMotion = CurrentToWorldSyncNorm * (Scale * StepTowardTarget);
		}

		// Put our result back in world space.  
		FinalVIRootMotion.SetTranslation(ToRootSyncSpace.TransformVector(SkewedVIRootMotion));
	}

	if(bWarpRotation)
	{
		const FQuat WarpedRotation = WarpRotation(OwnerComp, InVIRootMotion, VIRootMotionTotal, DeltaSeconds);
		FinalVIRootMotion.SetRotation(WarpedRotation);
	}

	// Debug
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const int32 DebugLevel = FVIMotionWarpingCVars::CVarVIMotionWarpingDebug.GetValueOnGameThread();
	if (DebugLevel > 0)
	{
		PrintLog(OwnerComp, TEXT("FVIRootMotionModifier_Skew"), InVIRootMotion, FinalVIRootMotion);

		if (DebugLevel >= 2)
		{
			const float DrawDebugDuration = FVIMotionWarpingCVars::CVarVIMotionWarpingDrawDebugDuration.GetValueOnGameThread();
			DrawDebugCoordinateSystem(OwnerComp.GetWorld(), CachedSyncPoint.GetLocation(), CachedSyncPoint.Rotator(), 50.f, false, DrawDebugDuration, 0, 1.f);
		}
	}
#endif

	return FinalVIRootMotion;
}

// UVIRootMotionModifierConfig_SkewWarp
///////////////////////////////////////////////////////////////

void UVIRootMotionModifierConfig_SkewWarp::AddVIRootMotionModifierSkewWarp(UVIMotionWarpingComponent* InVIMotionWarpingComp, const UAnimSequenceBase* InAnimation, float InStartTime, float InEndTime, FName InSyncPointName, bool bInWarpTranslation, bool bInIgnoreZAxis, bool bInWarpRotation, EVIMotionWarpRotationType InRotationType, float InWarpRotationTimeMultiplier)
{
	if (ensureAlways(InVIMotionWarpingComp))
	{
		TSharedPtr<FVIRootMotionModifier_SkewWarp> NewModifier = MakeShared<FVIRootMotionModifier_SkewWarp>();
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