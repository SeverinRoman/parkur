// Copyright (c) 2019-2022 Drowning Dragons Limited. All Rights Reserved.

#include "VIBlueprintFunctionLibrary.h"
#include "Kismet/GameplayStaticsTypes.h"
#include "Pawn/VICharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Pawn/VIPawnInterface.h"
#include "Stats/Stats.h"
#include "CollisionQueryParams.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "DrawDebugHelpers.h"

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("ComputeVault"), STAT_COMPUTEVAULT_COUNT, STATGROUP_VaultIt);
DECLARE_CYCLE_STAT(TEXT("ComputeVault"), STAT_COMPUTEVAULT, STATGROUP_VaultIt);

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("PredictCapsulePath"), STAT_PREDICTLANDINGLOCATION_COUNT, STATGROUP_VaultIt);
DECLARE_CYCLE_STAT(TEXT("PredictCapsulePath"), STAT_PREDICTLANDINGLOCATION, STATGROUP_VaultIt);

void UVIBlueprintFunctionLibrary::MessageLogError(const FString& ErrorMsg, bool bOpenLog /*= true*/)
{
	FMessageLog MsgLog("PIE");

	MsgLog.Error(FText::FromString(ErrorMsg));

	if (bOpenLog)
	{
		MsgLog.Open(EMessageSeverity::Error);
	}
}

float UVIBlueprintFunctionLibrary::GetMaxHeightFromGround(ACharacter* Character)
{
	if(!Character || !Character->GetCharacterMovement())
	{
		return 0.f;
	}

	return GetMaxHeightFromGroundForPawn(Character, Character->GetCharacterMovement()->GetGravityZ(), Character->GetCharacterMovement()->JumpZVelocity);
}

float UVIBlueprintFunctionLibrary::GetMaxVaultHeight(APawn* Pawn)
{
	if (Pawn)
	{
		if (Pawn->Implements<UVIPawnInterface>())
		{
			return IVIPawnInterface::Execute_GetVaultTraceSettings(Pawn).MaxLedgeHeight;
		}
		else
		{
			FMessageLog MsgLog("AssetCheck");

			MsgLog.Error(FText::FromString(FString::Printf(TEXT("UVIBlueprintFunctionLibrary::GetMaxHeightFromGroundForPawn failed: %s does not implement interface UVIPawnInterface"), *Pawn->GetName())));
			MsgLog.Open(EMessageSeverity::Error);
		}
	}

	return 0.f;
}

float UVIBlueprintFunctionLibrary::GetMaxHeightFromGroundForPawn(APawn* Pawn, const float Gravity, const float JumpZVelocity)
{
	return GetMaxJumpHeight(Gravity, JumpZVelocity) + GetMaxVaultHeight(Pawn);
}

float UVIBlueprintFunctionLibrary::GetMaxJumpHeightForCharacter(ACharacter* Character)
{
	if (Character && Character->GetCharacterMovement())
	{
		return GetMaxJumpHeight(Character->GetCharacterMovement()->GetGravityZ(), Character->GetCharacterMovement()->JumpZVelocity);
	}

	return 0.f;
}

bool UVIBlueprintFunctionLibrary::ActorIsAscending(AActor* InActor, bool bWorldUpIsZ)
{
	if (!InActor)
	{
		return false;
	}

	if (bWorldUpIsZ)
	{
		return InActor->GetVelocity().Z > 0.f;
	}

	return ComputeDirectionToFloat(InActor->GetVelocity(), InActor->GetActorUpVector()) > 0.f;
}

FVIVaultInfo UVIBlueprintFunctionLibrary::GetVaultInfoFromTargetData(const FGameplayAbilityTargetDataHandle& TargetData)
{
	if (TargetData.Data.IsValidIndex(0))
	{
		FGameplayAbilityTargetData* Data = TargetData.Data[0].Get();
		if (Data)
		{
			const FVIGameplayAbilityTargetData_VaultInfo* VaultData = (const FVIGameplayAbilityTargetData_VaultInfo*)(Data);
			const FVIVaultInfo* VaultInfoPtr = VaultData->GetVaultInfo();
			if (VaultInfoPtr)
			{
				return *VaultInfoPtr;
			}
		}
	}

	return FVIVaultInfo();
}

bool UVIBlueprintFunctionLibrary::PredictLandingLocation(FPredictProjectilePathResult& OutPredictResult, ACharacter* ForCharacter, const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes, bool bTraceComplex)
{
	if (!ForCharacter)
	{
		return false;
	}

	return PredictLandingLocation(
		OutPredictResult,
		ForCharacter,
		ObjectTypes,
		ForCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),
		ForCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius(),
		ForCharacter->GetCharacterMovement()->GetGravityZ(),
		bTraceComplex
	);
}

bool UVIBlueprintFunctionLibrary::PredictLandingLocation(FPredictProjectilePathResult& OutPredictResult, AActor* ForActor, const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes, float HalfHeight, float Radius, const FVector& Gravity, bool bTraceComplex)
{
	if (!ForActor)
	{
		return false;
	}

	const float GravityZ = ComputeDirectionToFloat(Gravity, ForActor->GetActorUpVector());
	return PredictLandingLocation(OutPredictResult, ForActor, ObjectTypes, HalfHeight, Radius, GravityZ, bTraceComplex);
}

bool UVIBlueprintFunctionLibrary::PredictLandingLocation(FPredictProjectilePathResult& OutPredictResult, AActor* ForActor, const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes, float HalfHeight, float Radius, float GravityZ, bool bTraceComplex)
{
	if (!ForActor)
	{
		return false;
	}

	// Performance profiling
	INC_DWORD_STAT(STAT_PREDICTLANDINGLOCATION_COUNT);
	SCOPE_CYCLE_COUNTER(STAT_PREDICTLANDINGLOCATION);

	Radius *= 0.98f;
	constexpr float MaxSimTime = 2.f;
	constexpr float SimFrequency = 15.f;
	const TArray<AActor*> TraceIgnore{ ForActor };

	FPredictProjectilePathParams Params = FPredictProjectilePathParams(Radius, ForActor->GetActorLocation(), ForActor->GetVelocity(), MaxSimTime);
	Params.bTraceWithCollision = true;
	Params.bTraceComplex = bTraceComplex;
	Params.ActorsToIgnore = TraceIgnore;
	Params.SimFrequency = SimFrequency;
	Params.OverrideGravityZ = GravityZ;
	Params.ObjectTypes = ObjectTypes;

	// Do the trace
	if (!PredictCapsulePath(ForActor, HalfHeight, Params, OutPredictResult, ForActor->GetActorUpVector()))
	{
		return false;
	}
	return true;
}

bool UVIBlueprintFunctionLibrary::PredictCapsulePath(const UObject* WorldContextObject, float HalfHeight, const struct FPredictProjectilePathParams& PredictParams, struct FPredictProjectilePathResult& PredictResult, const FVector& UpVector)
{
	PredictResult.Reset();
	bool bBlockingHit = false;

	UWorld const* const World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (World && PredictParams.SimFrequency > KINDA_SMALL_NUMBER)
	{
		const float SubstepDeltaTime = 1.f / PredictParams.SimFrequency;
		const float GravityZ = FMath::IsNearlyEqual(PredictParams.OverrideGravityZ, 0.0f) ? World->GetGravityZ() : PredictParams.OverrideGravityZ;
		const float ProjectileRadius = PredictParams.ProjectileRadius;

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PredictProjectilePath), PredictParams.bTraceComplex);
		FCollisionObjectQueryParams ObjQueryParams;
		const bool bTraceWithObjectType = (PredictParams.ObjectTypes.Num() > 0);
		const bool bTracePath = PredictParams.bTraceWithCollision && (PredictParams.bTraceWithChannel || bTraceWithObjectType);
		if (bTracePath)
		{
			QueryParams.AddIgnoredActors(PredictParams.ActorsToIgnore);
			if (bTraceWithObjectType)
			{
				for (auto Iter = PredictParams.ObjectTypes.CreateConstIterator(); Iter; ++Iter)
				{
					const ECollisionChannel& Channel = UCollisionProfile::Get()->ConvertToCollisionChannel(false, *Iter);
					ObjQueryParams.AddObjectTypesToQuery(Channel);
				}
			}
		}

		FVector CurrentVel = PredictParams.LaunchVelocity;
		FVector TraceStart = PredictParams.StartLocation;
		FVector TraceEnd = TraceStart;
		float CurrentTime = 0.f;
		PredictResult.PathData.Reserve(FMath::Min(128, FMath::CeilToInt(PredictParams.MaxSimTime * PredictParams.SimFrequency)));
		PredictResult.AddPoint(TraceStart, CurrentVel, CurrentTime);

		FHitResult ObjectTraceHit(NoInit);
		FHitResult ChannelTraceHit(NoInit);
		ObjectTraceHit.Time = 1.f;
		ChannelTraceHit.Time = 1.f;

		const float MaxSimTime = PredictParams.MaxSimTime;
		while (CurrentTime < MaxSimTime)
		{
			// Limit step to not go further than total time.
			const float PreviousTime = CurrentTime;
			const float ActualStepDeltaTime = FMath::Min(MaxSimTime - CurrentTime, SubstepDeltaTime);
			CurrentTime += ActualStepDeltaTime;

			// Integrate (Velocity Verlet method)
			TraceStart = TraceEnd;
			FVector OldVelocity = CurrentVel;
			CurrentVel = OldVelocity + (UpVector * GravityZ * ActualStepDeltaTime);
			//CurrentVel = OldVelocity + FVector(0.f, 0.f, GravityZ * ActualStepDeltaTime);
			TraceEnd = TraceStart + (OldVelocity + CurrentVel) * (0.5f * ActualStepDeltaTime);
			PredictResult.LastTraceDestination.Set(TraceEnd, CurrentVel, CurrentTime);

			if (bTracePath)
			{
				bool bObjectHit = false;
				bool bChannelHit = false;
				if (bTraceWithObjectType)
				{
					bObjectHit = World->SweepSingleByObjectType(ObjectTraceHit, TraceStart, TraceEnd, FQuat::Identity, ObjQueryParams, FCollisionShape::MakeCapsule(ProjectileRadius, HalfHeight), QueryParams);
				}
				if (PredictParams.bTraceWithChannel)
				{
					bChannelHit = World->SweepSingleByChannel(ChannelTraceHit, TraceStart, TraceEnd, FQuat::Identity, PredictParams.TraceChannel, FCollisionShape::MakeCapsule(ProjectileRadius, HalfHeight), QueryParams);
				}

				// See if there were any hits.
				if (bObjectHit || bChannelHit)
				{
					// Hit! We are done. Choose trace with earliest hit time.
					PredictResult.HitResult = (ObjectTraceHit.Time < ChannelTraceHit.Time) ? ObjectTraceHit : ChannelTraceHit;
					const float HitTimeDelta = ActualStepDeltaTime * PredictResult.HitResult.Time;
					const float TotalTimeAtHit = PreviousTime + HitTimeDelta;
					const FVector VelocityAtHit = OldVelocity + (UpVector * GravityZ * HitTimeDelta);
					//const FVector VelocityAtHit = OldVelocity + FVector(0.f, 0.f, GravityZ * HitTimeDelta);
					PredictResult.AddPoint(PredictResult.HitResult.Location, VelocityAtHit, TotalTimeAtHit);
					bBlockingHit = true;
					break;
				}
			}

			PredictResult.AddPoint(TraceEnd, CurrentVel, CurrentTime);
		}

		// Draw debug path
#if ENABLE_DRAW_DEBUG
		if (PredictParams.DrawDebugType != EDrawDebugTrace::None)
		{
			const bool bPersistent = PredictParams.DrawDebugType == EDrawDebugTrace::Persistent;
			const float LifeTime = (PredictParams.DrawDebugType == EDrawDebugTrace::ForDuration) ? PredictParams.DrawDebugTime : 0.f;
			const float DrawRadius = (ProjectileRadius > 0.f) ? ProjectileRadius : 5.f;

			// draw the path
			for (const FPredictProjectilePathPointData& PathPt : PredictResult.PathData)
			{
				::DrawDebugCapsule(World, PathPt.Location, HalfHeight, DrawRadius, FQuat::Identity, FColor::Green, bPersistent, LifeTime);
			}
			// draw the impact point
			if (bBlockingHit)
			{
				::DrawDebugCapsule(World, PredictResult.HitResult.Location, HalfHeight, DrawRadius + 1.0f, FQuat::Identity, FColor::Red, bPersistent, LifeTime);
			}
		}
#endif //ENABLE_DRAW_DEBUG
	}

	return bBlockingHit;
}

FVIVaultResult UVIBlueprintFunctionLibrary::ComputeVault(APawn* const Pawn, const FVector& InVaultDirection, const FVITraceSettings& TraceSettings, const FVICapsuleInfo& Capsule, bool bTraceComplex)
{
	FVIVaultResult Result = FVIVaultResult();

	if (!IsValid(Pawn))
	{
		// Invalid
		return Result;
	}

	if (!Pawn->Implements<UVIPawnInterface>())
	{
		// Needs pawn interface
		FMessageLog MsgLog("PIE");

		MsgLog.Error(FText::FromString(FString::Printf(TEXT("ComputeVault() failed: %s does not implement interface UVIPawnInterface"), *Pawn->GetName())));
		MsgLog.Open(EMessageSeverity::Error);
		return Result;
	}

	const FVector& VaultDirection = InVaultDirection.GetSafeNormal();
	if (VaultDirection.IsNearlyZero())
	{
		// Do not have a usable vector
		return Result;
	}

	// Performance profiling
	INC_DWORD_STAT(STAT_COMPUTEVAULT_COUNT);
	SCOPE_CYCLE_COUNTER(STAT_COMPUTEVAULT);

	// Ease of access
	const float HalfHeight = Capsule.HalfHeight;
	const float Radius = Capsule.Radius;

	// Adding 1.f overcomes issues with step height and inconsistencies with MaxLedgeHeight
	const float MaxLedgeHeight = TraceSettings.MaxLedgeHeight + 1.f;
	const float MinLedgeHeight = TraceSettings.MinLedgeHeight + 1.f;

	const TArray<AActor*> TraceIgnore = { Pawn };

	// Cache properties
	const float HeightOffset = HalfHeight + TraceSettings.CollisionFloatHeight;
	const FVector& Up = Pawn->GetActorUpVector();
	const FVector& Loc = Pawn->GetActorLocation();
	const FVector BaseLoc = Loc - (Up * HeightOffset) - (Up * Radius * 0.05f);

	// Trace forward to find something not walkable; don't climb something character can simply walk on
	FHitResult NotWalkableHit(ForceInit);
	{
		// Cache trace
		const FVector TraceStart = BaseLoc + Up * ((MaxLedgeHeight + MinLedgeHeight) * 0.5f);
		const FVector TraceEnd = TraceStart + (VaultDirection * TraceSettings.ReachDistance);
		const float TraceHalfHeight = 1.f + ((MaxLedgeHeight - MinLedgeHeight) * 0.5f);

		CapsuleTraceSingleForObjects(
		Pawn,																	// World Context
		TraceStart,																// Start
		TraceEnd,																// End
		Pawn->GetActorQuat(),													// Rotation
		TraceSettings.ForwardTraceRadius,										// Radius
		TraceHalfHeight,														// HalfHeight
		TraceSettings.GetObjectTypes(),											// TraceChannels
		bTraceComplex,															// bTraceComplex
		TraceIgnore,															// ActorsToIgnore
		false,																	// bDrawDebug (ForDuration)
		NotWalkableHit,															// OutHit
		false,																	// bIgnoreSelf
		FLinearColor::Red,														// TraceColor
		FLinearColor::Green,													// TraceHitColor
		1.f																		// DrawTime
		);

		if (!NotWalkableHit.IsValidBlockingHit() || IVIPawnInterface::Execute_IsWalkable(Pawn, NotWalkableHit))
		{
			// IsValidBlockingHit() returns false if no hit or starting in penetration
			return Result;
		}
	}

	// Abort if object is moving too fast to vault onto
	if (TraceSettings.MaxObjectVelocity > 0.f && NotWalkableHit.GetActor()->GetVelocity().Size() > TraceSettings.MaxObjectVelocity)
	{
		return Result;
	}

	// Determined whats in front of us isn't walkable so lets check if we can stand on it
	// Trace downward from first trace to ensure surface is walkable
	FHitResult GroundHit(ForceInit);
	{
		const FVector ImpactLoc = NotWalkableHit.ImpactPoint;
		const FVector ImpactNormal = NotWalkableHit.ImpactNormal;

		const FVector TraceFwd = FVector::VectorPlaneProject(ImpactLoc, Up);
		const FVector TraceUp = BaseLoc.ProjectOnTo(Up);

		const FVector TraceEnd = TraceFwd + TraceUp + (ImpactNormal * -15.f);
		const FVector TraceStart = TraceEnd + (Up * (MaxLedgeHeight + TraceSettings.DownwardTraceRadius + 1.f));

		UKismetSystemLibrary::SphereTraceSingleForObjects(
		Pawn,																	// World Context
		TraceStart,																// Start
		TraceEnd,																// End
		TraceSettings.DownwardTraceRadius,										// Radius
		TraceSettings.GetObjectTypes(),											// TraceChannels
		bTraceComplex,															// bTraceComplex
		TraceIgnore,															// ActorsToIgnore
		EDrawDebugTrace::None,													// DrawDebugType
		GroundHit,																// OutHit
		false,																	// bIgnoreSelf
		FLinearColor::Yellow,													// TraceColor
		FLinearColor::Blue,														// TraceHitColor
		1.f																		// DrawTime
		);

		// If we can't walk on the surface don't try to vault onto it
		if (!IVIPawnInterface::Execute_IsWalkable(Pawn, GroundHit))
		{
			return Result;
		}
	}

	// Ensure capsule can fit at location
	const FVector GroundLoc = FVector(GroundHit.Location.X, GroundHit.Location.Y, GroundHit.ImpactPoint.Z) + (Up * HeightOffset);
	FHitResult Hit(1.f);
	{
		// Identical to GetCapsuleComponent()->GetScaledCapsuleHalfHeight_WithoutHemisphere()
		const float HalfHeightNoHemisphere = HalfHeight - Radius;

		const FVector TraceStart = GroundLoc + (Up * HalfHeightNoHemisphere);
		const FVector TraceEnd = GroundLoc - (Up * HalfHeightNoHemisphere);

		UKismetSystemLibrary::SphereTraceSingleByProfile(
		Pawn,																	// World Context
		TraceStart,																// Start
		TraceEnd,																// End
		Radius,																	// Radius
		TraceSettings.TraceProfile,												// ProfileName
		bTraceComplex,															// bTraceComplex
		TraceIgnore,															// ActorsToIgnore
		EDrawDebugTrace::None,													// DrawDebugType
		Hit,																	// OutHit
		false,																	// bIgnoreSelf
		FLinearColor::Yellow,													// TraceColor
		FLinearColor::Blue,														// TraceHitColor
		1.f																		// DrawTime
		);

		// No room if an obstacle is in the way
		if (Hit.IsValidBlockingHit())
		{
			return Result;
		}
	}

	// Can vault
	Result.bSuccess = true;
	Result.Location = GroundLoc;
	Result.Direction = -FVector::VectorPlaneProject(NotWalkableHit.ImpactNormal, Up);
	Result.Height = (GroundLoc - Loc).ProjectOnTo(Up).Size();

	//DrawDebugSphere(Pawn->GetWorld(), GroundLoc, 32.f, 16, FColor::White, true);
	//DrawDebugDirectionalArrow(Pawn->GetWorld(), GroundLoc, GroundLoc + Result.Direction * 200.f, 40.f, FColor::Green, true, -1.f, 0, 2.f);

	return Result;
}

FCollisionQueryParams VIConfigureCollisionParams(FName TraceTag, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, bool bIgnoreSelf, const UObject* WorldContextObject)
{
	FCollisionQueryParams Params(TraceTag, SCENE_QUERY_STAT_ONLY(KismetTraceUtils), bTraceComplex);
	Params.bReturnPhysicalMaterial = true;
	Params.bReturnFaceIndex = !UPhysicsSettings::Get()->bSuppressFaceRemapTable; // Ask for face index, as long as we didn't disable globally
	Params.AddIgnoredActors(ActorsToIgnore);
	if (bIgnoreSelf)
	{
		const AActor* IgnoreActor = Cast<AActor>(WorldContextObject);
		if (IgnoreActor)
		{
			Params.AddIgnoredActor(IgnoreActor);
		}
		else
		{
			// find owner
			const UObject* CurrentObject = WorldContextObject;
			while (CurrentObject)
			{
				CurrentObject = CurrentObject->GetOuter();
				IgnoreActor = Cast<AActor>(CurrentObject);
				if (IgnoreActor)
				{
					Params.AddIgnoredActor(IgnoreActor);
					break;
				}
			}
		}
	}

	return Params;
}

FCollisionObjectQueryParams VIConfigureCollisionObjectParams(const TArray<TEnumAsByte<EObjectTypeQuery> >& ObjectTypes)
{
	TArray<TEnumAsByte<ECollisionChannel>> CollisionObjectTraces;
	CollisionObjectTraces.AddUninitialized(ObjectTypes.Num());

	for (auto Iter = ObjectTypes.CreateConstIterator(); Iter; ++Iter)
	{
		CollisionObjectTraces[Iter.GetIndex()] = UEngineTypes::ConvertToCollisionChannel(*Iter);
	}

	FCollisionObjectQueryParams ObjectParams;
	for (auto Iter = CollisionObjectTraces.CreateConstIterator(); Iter; ++Iter)
	{
		const ECollisionChannel& Channel = (*Iter);
		if (FCollisionObjectQueryParams::IsValidObjectQuery(Channel))
		{
			ObjectParams.AddObjectTypesToQuery(Channel);
		}
		else
		{
			UE_LOG(LogBlueprintUserMessages, Warning, TEXT("%d isn't valid object type"), (int32)Channel);
		}
	}

	return ObjectParams;
}

void UVIBlueprintFunctionLibrary::CapsuleTraceSingleForObjects(const UObject* WorldContextObject, const FVector Start, const FVector End, const FQuat Rot, float Radius, float HalfHeight, const TArray<TEnumAsByte<EObjectTypeQuery> >& ObjectTypes, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, bool bDrawDebug, FHitResult& OutHit, bool bIgnoreSelf, FLinearColor TraceColor /*= FLinearColor::Red*/, FLinearColor TraceHitColor /*= FLinearColor::Green*/, float DrawTime /*= 5.0f*/)
{
	static const FName CapsuleTraceSingleName(TEXT("CapsuleTraceSingleForObjects"));
	const FCollisionQueryParams Params = VIConfigureCollisionParams(CapsuleTraceSingleName, bTraceComplex, ActorsToIgnore, bIgnoreSelf, WorldContextObject);

	const FCollisionObjectQueryParams ObjectParams = VIConfigureCollisionObjectParams(ObjectTypes);
	if (ObjectParams.IsValid() == false)
	{
		UE_LOG(LogBlueprintUserMessages, Warning, TEXT("Invalid object types"));
		return;
	}

	const UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	bool const bHit = World ? World->SweepSingleByObjectType(OutHit, Start, End, Rot, ObjectParams, FCollisionShape::MakeCapsule(Radius, HalfHeight), Params) : false;

#if ENABLE_DRAW_DEBUG
	DrawDebugCapsuleTraceSingle(World, Start, End, Rot, Radius, HalfHeight, bDrawDebug, bHit, OutHit, TraceColor, TraceHitColor, DrawTime);
#endif
}

void UVIBlueprintFunctionLibrary::DrawDebugCapsuleTraceSingle(const UWorld* World, const FVector& Start, const FVector& End, const FQuat& Rot, float Radius, float HalfHeight, bool bDrawDebug, bool bHit, const FHitResult& OutHit, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	if (bDrawDebug)
	{
		constexpr bool bPersistent = false;
		const float LifeTime = DrawTime;

		if (bHit && OutHit.bBlockingHit)
		{
			// Red up to the blocking hit, green thereafter
			DrawDebugCapsule(World, Start, HalfHeight, Radius, Rot, TraceColor.ToFColor(true), bPersistent, LifeTime);
			DrawDebugCapsule(World, OutHit.Location, HalfHeight, Radius, Rot, TraceColor.ToFColor(true), bPersistent, LifeTime);
			DrawDebugLine(World, Start, OutHit.Location, TraceColor.ToFColor(true), bPersistent, LifeTime);
			DrawDebugPoint(World, OutHit.ImpactPoint, 16.f, TraceColor.ToFColor(true), bPersistent, LifeTime);

			DrawDebugCapsule(World, End, HalfHeight, Radius, Rot, TraceHitColor.ToFColor(true), bPersistent, LifeTime);
			DrawDebugLine(World, OutHit.Location, End, TraceHitColor.ToFColor(true), bPersistent, LifeTime);
		}
		else
		{
			// no hit means all red
			DrawDebugCapsule(World, Start, HalfHeight, Radius, Rot, TraceColor.ToFColor(true), bPersistent, LifeTime);
			DrawDebugCapsule(World, End, HalfHeight, Radius, Rot, TraceColor.ToFColor(true), bPersistent, LifeTime);
			DrawDebugLine(World, Start, End, TraceColor.ToFColor(true), bPersistent, LifeTime);
		}
	}
}

float UVIBlueprintFunctionLibrary::ComputeDirectionToFloat(const FVector& Vector, const FVector& Dir)
{
	const FVector VectorProject = Vector.ProjectOnTo(Dir);
	const float SignDirection = FMath::Sign(VectorProject | Dir);

	return VectorProject.Size() * SignDirection;
}
