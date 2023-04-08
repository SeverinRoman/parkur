// Copyright (c) 2019-2022 Drowning Dragons Limited. All Rights Reserved.

#include "Animation/VIAnimNotifyState_FBIK.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/VIAnimationInterface.h"
#include "Pawn/VIPawnInterface.h"
#include "VIBlueprintFunctionLibrary.h"
#include "GameFramework/Pawn.h"
#include "Kismet/KismetSystemLibrary.h"

DECLARE_CYCLE_STAT(TEXT("Update FBIK"), STAT_VAULTFBIK, STATGROUP_VaultIt);

#if WITH_EDITOR
void UVIAnimNotifyState_FBIK::OnAnimNotifyCreatedInEditor(FAnimNotifyEvent& ContainingAnimNotifyEvent)
{
	ContainingAnimNotifyEvent.bTriggerOnDedicatedServer = false;
	ContainingAnimNotifyEvent.NotifyFilterType = ENotifyFilterType::LOD;
	ContainingAnimNotifyEvent.NotifyFilterLOD = 2;
}
#endif  // WITH_EDITOR

void UVIAnimNotifyState_FBIK::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration)
{
	// Compute LateralOffset
	APawn* const PawnOwner = GetPawnOwner(MeshComp);
	if (!PawnOwner)
	{
		return;
	}

	if (!HasValidRole(PawnOwner))
	{
		return;
	}

	LateralOffset = 0.f;
	SkippedTicks = TickSkipAmount;

	const FVector& Loc = PawnOwner->GetActorLocation();
	const FVector& BoneLoc = MeshComp->GetSocketLocation(BoneName);

	LateralOffset = UVIBlueprintFunctionLibrary::ComputeDirectionToFloat((BoneLoc - Loc), PawnOwner->GetActorRightVector());

	if (UpdateType == EVIFBIKUpdateType::FUT_Single)
	{
		UpdateFBIK(MeshComp);
	}
}

void UVIAnimNotifyState_FBIK::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime)
{
	if (UpdateType == EVIFBIKUpdateType::FUT_Tick)
	{
		UpdateFBIK(MeshComp);
	}
}

void UVIAnimNotifyState_FBIK::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	if (MeshComp && MeshComp->GetAnimInstance())
	{
		APawn* const PawnOwner = GetPawnOwner(MeshComp);
		if (!PawnOwner)
		{
			return;
		}

		if (!HasValidRole(PawnOwner))
		{
			return;
		}

		LateralOffset = 0.f;
		SkippedTicks = TickSkipAmount;

		if (MeshComp->GetAnimInstance()->Implements<UVIAnimationInterface>())
		{
			// Disable FBIK
			IVIAnimationInterface::Execute_SetBoneFBIK(MeshComp->GetAnimInstance(), BoneName, FVector::ZeroVector, false);
		}
		else
		{
			UVIBlueprintFunctionLibrary::MessageLogError(FString::Printf(TEXT("{ %s } does not implement interface VIAnimationInterface. Aborting FBIK update."), *MeshComp->GetAnimInstance()->GetName()));
		}
	}
}

void UVIAnimNotifyState_FBIK::UpdateFBIK(USkeletalMeshComponent* MeshComp)
{
	if (MeshComp && MeshComp->GetAnimInstance())
	{
		if (LateralOffset == 0.f)
		{
			return;
		}

		APawn* const PawnOwner = GetPawnOwner(MeshComp);
		if (!PawnOwner)
		{
			return;
		}

		if (!HasValidRole(PawnOwner))
		{
			return;
		}

		SCOPE_CYCLE_COUNTER(STAT_VAULTFBIK);

		// Skip ticks to reduce overhead
		// (as this is only cosmetic and old values can be safely used for the insignificant amount of time lapsed as values are interpolated anyway)
		if (SkippedTicks != TickSkipAmount)
		{
			SkippedTicks++;
			return;
		}
		else
		{
			SkippedTicks = 0;
		}

		if (!PawnOwner->Implements<UVIPawnInterface>())
		{
			UVIBlueprintFunctionLibrary::MessageLogError(FString::Printf(TEXT("{ %s } does not implement interface VIPawnInterface. Aborting FBIK update."), *PawnOwner->GetName()));
			return;
		}

		if (MeshComp->GetAnimInstance()->Implements<UVIAnimationInterface>())
		{
			// Perform the trace
			FHitResult Hit(ForceInit);
			{
				const FVITraceSettings& TraceSettings = IVIPawnInterface::Execute_GetVaultTraceSettings(PawnOwner);
				const TArray<AActor*> TraceIgnore = { PawnOwner };

				FVector VaultLoc;
				FVector VaultDir;
				IVIPawnInterface::Execute_GetVaultLocationAndDirection(PawnOwner, VaultLoc, VaultDir);

				const FVector& Right = PawnOwner->GetActorRightVector();
				const FVector RightLoc = PawnOwner->GetActorLocation() + (Right * LateralOffset);
				const FVector VaultRightLoc = VaultLoc + (Right * LateralOffset);

				const FVector& TraceStart = RightLoc;
				const FVector TraceEnd = VaultRightLoc + (VaultDir * TraceLength);

				bool bTraceComplex = (TraceType != EVIFBIKTraceType::FTT_Simple);
				if (TraceType == EVIFBIKTraceType::FTT_ComplexLocalOnly)
				{
					bTraceComplex = IsLocalPlayer(PawnOwner);
				}

#if WITH_EDITOR
				const EDrawDebugTrace::Type DrawDebugType = (bDebugTraceDuringPIE) ? ((UpdateType == EVIFBIKUpdateType::FUT_Single) ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::ForOneFrame) : EDrawDebugTrace::None;
#else
				const EDrawDebugTrace::Type DrawDebugType = EDrawDebugTrace::None;
#endif  // WITH_EDITOR

				UKismetSystemLibrary::SphereTraceSingleForObjects(
					PawnOwner,																// World Context
					TraceStart,																// Start
					TraceEnd,																// End
					TraceRadius,															// Radius
					TraceSettings.GetObjectTypes(),											// TraceChannels
					bTraceComplex,															// bTraceComplex
					TraceIgnore,															// ActorsToIgnore
					DrawDebugType,															// DrawDebugType
					Hit,																	// OutHit
					false,																	// bIgnoreSelf
					FLinearColor::Yellow,													// TraceColor
					FLinearColor::Blue,														// TraceHitColor
					1.f																		// DrawTime
				);

				if (Hit.bBlockingHit)
				{
					Hit.Location += Hit.ImpactNormal * BoneOffset;
				}
			}

			if (Hit.bBlockingHit)
			{
				IVIAnimationInterface::Execute_SetBoneFBIK(MeshComp->GetAnimInstance(), BoneName, Hit.Location, true);
			}
			else if (bDisableIfHitFails)
			{
				// We generally leave it enabled if there was no hit and just use the last successful hit
				// but here the user has specified to disable it
				IVIAnimationInterface::Execute_SetBoneFBIK(MeshComp->GetAnimInstance(), BoneName, FVector::ZeroVector, false);
			}
		}
		else
		{
			UVIBlueprintFunctionLibrary::MessageLogError(FString::Printf(TEXT("{ %s } does not implement interface VIAnimationInterface. Aborting FBIK update."), *MeshComp->GetAnimInstance()->GetName()));
		}
	}
}

APawn* UVIAnimNotifyState_FBIK::GetPawnOwner(const USkeletalMeshComponent* const MeshComp)
{
	if (MeshComp && MeshComp->GetOwner())
	{
		return MeshComp->GetOwner<APawn>();
	}

	return nullptr;
}

bool UVIAnimNotifyState_FBIK::HasValidRole(APawn* const PawnOwner) const
{
	switch (ApplyToRoles)
	{
		case EVIFBIKUpdateRole::FUR_LocalOnly:
			if (IsLocalPlayer(PawnOwner))
			{
				return true;
			}
			return false;
		case EVIFBIKUpdateRole::FUR_SimulatedOnly:
			if (PawnOwner->GetLocalRole() == ROLE_SimulatedProxy)
			{
				return true;
			}
			return false;
		case EVIFBIKUpdateRole::FUR_All:
		default:
			break;
	}

	return true;
}

bool UVIAnimNotifyState_FBIK::IsLocalPlayer(const APawn* const PawnOwner)
{
	return PawnOwner && PawnOwner->IsLocallyControlled() && (PawnOwner->GetNetMode() == NM_Standalone || !PawnOwner->HasAuthority());
}
