// Copyright (c) 2019-2022 Drowning Dragons Limited. All Rights Reserved.

#include "Pawn/VIPawnVaultComponent.h"
#include "VIBlueprintFunctionLibrary.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "AbilitySystemInterface.h"
#include "GAS/VIAbilitySystemComponent.h"
#include "Pawn/VIPawnInterface.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Kismet/GameplayStaticsTypes.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

DECLARE_CYCLE_STAT(TEXT("AutoVault"), STAT_VAULTAUTOVAULT, STATGROUP_VaultIt);

DEFINE_LOG_CATEGORY_STATIC(LogVaultItAntiCheat, Log, All);

void UVIPawnVaultComponent::BeginPlay()
{
	Super::BeginPlay();

	// Cache PawnOwner
	PawnOwner = (GetOwner()) ? GetOwner<APawn>() : nullptr;

	if (PawnOwner)
	{
		// Cache CapsuleInfo
		CapsuleInfo = GetCapsuleInfo();

		// Cache ASC
		if (const IAbilitySystemInterface* const Interface = Cast<IAbilitySystemInterface>(PawnOwner))
		{
			ASC = Interface->GetAbilitySystemComponent();
			if (!ASC)
			{
				UVIBlueprintFunctionLibrary::MessageLogError(FString::Printf(TEXT("UVIPawnVaultComponent::BeginPlay %s IAbilitySystemInterface does not return a valid UAbilitySystemComponent. Vault will not work."), *PawnOwner->GetName()));
			}
		}
		else
		{
			UVIBlueprintFunctionLibrary::MessageLogError(FString::Printf(TEXT("UVIPawnVaultComponent::BeginPlay %s does not implement interface IAbilitySystemInterface. Vault will not work."), *PawnOwner->GetName()));
			return;
		}
	}

	// Grant Vault Ability to PawnOwner ASC (on server / standalone)
	// Has a lot of error logging if anything is invalid to reduce user error
	if (PawnOwner->GetLocalRole() == ROLE_Authority && !bVaultAbilityInitialized)
	{
		if (VaultAbility)
		{
			if (VaultAbilityTag.IsValid())
			{
				bVaultAbilityInitialized = true;
				ASC->GiveAbility(FGameplayAbilitySpec(VaultAbility, 1, INDEX_NONE, PawnOwner));
			}
			else
			{
				UVIBlueprintFunctionLibrary::MessageLogError(FString::Printf(TEXT("VaultTag not assigned for { %s }"), *GetName()));
			}
		}
		else
		{
			UVIBlueprintFunctionLibrary::MessageLogError(FString::Printf(TEXT("VaultAbility class not assigned for { %s }"), *GetName()));
		}

		if (!VaultStateTag.IsValid())
		{
			UVIBlueprintFunctionLibrary::MessageLogError(FString::Printf(TEXT("VaultStateTag not assigned for { %s }"), *GetName()));
		}

		if (!VaultRemovalTag.IsValid())
		{
			UVIBlueprintFunctionLibrary::MessageLogError(FString::Printf(TEXT("VaultRemovalTag not assigned for { %s }"), *GetName()));
		}
	}

	AutoVaultSkippedTicks = AutoVaultCheckSkip;
}

bool UVIPawnVaultComponent::Jump(float GravityZ /* = 0.f */, bool bCanJump /* = false */, bool bIsFalling /* = false */)
{
	bLastJumpInputVaulted = false;

	switch (JumpKeyPriority)
	{
		case EVIJumpKeyPriority::JKP_SelectHighestPoint:
			// Test to see if jump is likely to succeed, otherwise vault
			if (IVIPawnInterface::Execute_CanVault(PawnOwner))
			{
				PendingVaultResult = ComputeVault();
				if (PendingVaultResult.bSuccess)
				{
					// Have a surface we can land on from the result
					// now test if jumping will get us there instead
					// by testing against the vertical height of predicted
					// landing location

					FPredictProjectilePathResult PathResult = FPredictProjectilePathResult();
					bool bUseJump = PredictLandingLocation(PathResult, CapsuleInfo.HalfHeight, CapsuleInfo.Radius, GravityZ);

					if (bUseJump)
					{
						const FVector& Up = PawnOwner->GetActorUpVector();

						const FVector& LandLoc = PathResult.HitResult.Location;
						const FVector VaultLoc = PendingVaultResult.Location - (Up * CapsuleInfo.HalfHeight);

						const float LandZ = UVIBlueprintFunctionLibrary::ComputeDirectionToFloat(LandLoc, Up);
						const float VaultZ = UVIBlueprintFunctionLibrary::ComputeDirectionToFloat(VaultLoc, Up);

						if (LandZ < VaultZ)
						{
							bUseJump = false;
						}
					}

					if (bUseJump)
					{
						PendingVaultResult = FVIVaultResult();
						return true;
					}
					else
					{
						bLastJumpInputVaulted = true;
						return false;
					}
				}
				else
				{
					return true;
				}
			}
			else
			{
				return true;
			}
			break;
		case EVIJumpKeyPriority::JKP_AlwaysVault:
			// If it can vault it will, otherwise jump
			if (IVIPawnInterface::Execute_CanVault(PawnOwner))
			{
				PendingVaultResult = ComputeVault();
				if (PendingVaultResult.bSuccess)
				{
					bLastJumpInputVaulted = true;
					return false;
				}
				else
				{
					PendingVaultResult = FVIVaultResult();
					return true;
				}
			}
			break;
		case EVIJumpKeyPriority::JKP_OnlyVaultFromAir:
			// Determine if jump key can do anything
			if (bIsFalling && !bCanJump)
			{
				bLastJumpInputVaulted = true;
				return false;
			}
			else
			{
				return true;
			}
			break;
		case EVIJumpKeyPriority::JKP_DisableVault:
		default:
			break;
	}

	return true;
}

void UVIPawnVaultComponent::StopJumping()
{
	if (bLastJumpInputVaulted)
	{
		StopVault();
	}
}

void UVIPawnVaultComponent::Vault()
{
	bPressedVault = true;
}

void UVIPawnVaultComponent::StopVault()
{
	bPressedVault = false;
}

void UVIPawnVaultComponent::CheckVaultInput(float DeltaTime, TEnumAsByte<enum EMovementMode> MovementMode /* = MOVE_Walking */)
{
	if (PawnOwner && PawnOwner->IsLocallyControlled())
	{
		// Compute AutoVault
		bool bAutoVault = false;
		if (!bPressedVault)
		{
			// Skip ticks optionally to reduce overhead
			if (AutoVaultSkippedTicks == AutoVaultCheckSkip)
			{
				// Waste of resources to check if already vaulting
				if (!IsVaulting() && AutoVaultStates != (uint8)EVIAutoVault::VIAV_None)
				{
					// Compare bit flags to see if we are in a state we're allowed to auto-vault from
					switch (MovementMode)
					{
					case MOVE_Walking:
					case MOVE_NavWalking:
						bAutoVault = (AutoVaultStates & (uint8)EVIAutoVault::VIAV_Walking) != 0;
						break;
					case MOVE_Falling:
						bAutoVault = (AutoVaultStates & (uint8)EVIAutoVault::VIAV_Falling) != 0;
						break;
					case MOVE_Swimming:
						bAutoVault = (AutoVaultStates & (uint8)EVIAutoVault::VIAV_Swimming) != 0;
						break;
					case MOVE_Flying:
						bAutoVault = (AutoVaultStates & (uint8)EVIAutoVault::VIAV_Flying) != 0;
						break;
					case MOVE_Custom:
						bAutoVault = (AutoVaultStates & (uint8)EVIAutoVault::VIAV_Custom) != 0;
						if (bAutoVault)
						{
							bAutoVault = IVIPawnInterface::Execute_CanAutoVaultInCustomMovementMode(PawnOwner);
						}
					default:
						break;
					}

					if (bAutoVault)
					{
						// Perform traces to test if we should auto vault
						bAutoVault &= ComputeShouldAutoVault();
					}
				}

				AutoVaultSkippedTicks = 0;
			}
			else
			{
				AutoVaultSkippedTicks++;
			}
		}

		if (bPressedVault || bAutoVault)
		{
			if (IVIPawnInterface::Execute_CanVault(PawnOwner))
			{
				const FVIVaultResult& VaultResult = (PendingVaultResult.IsValid()) ? PendingVaultResult : ComputeVault();

				if (VaultResult.bSuccess)
				{
					// Consume input if required
					if (AutoReleaseVaultInput != EVIVaultInputRelease::VIR_Never) { bPressedVault = false; }

					// Send vault result as info through EventData
					FVIGameplayAbilityTargetData_VaultInfo Info;
					Info.VaultInfo = ComputeVaultInfoFromResult(VaultResult);

					// Cache gameplay ability event data to be sent
					FGameplayEventData EventData;
					EventData.Instigator = PawnOwner;
					EventData.TargetData.Add(new FVIGameplayAbilityTargetData_VaultInfo(Info));

					// Trigger ability and send event data to ability & server
					UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(PawnOwner, VaultAbilityTag, EventData);

					// Send to Pawn to use for FBIK (or anything extended by user)
					IVIPawnInterface::Execute_OnLocalPlayerVault(PawnOwner, VaultResult.Location, VaultResult.Direction);
				}
			}
		}
	}

	if (AutoReleaseVaultInput == EVIVaultInputRelease::VIR_Always)
	{
		bPressedVault = false;
	}

	// Clear at end of frame always; triggered by jump key
	PendingVaultResult = FVIVaultResult();
}

bool UVIPawnVaultComponent::IsVaulting() const
{
	if (ASC)
	{
		return ASC->GetTagCount(VaultStateTag) > ASC->GetTagCount(VaultRemovalTag);
	}

	return false;
}

FVIVaultResult UVIPawnVaultComponent::ComputeVault() const
{
	return UVIBlueprintFunctionLibrary::ComputeVault(PawnOwner, IVIPawnInterface::Execute_GetVaultDirection(PawnOwner), IVIPawnInterface::Execute_GetVaultTraceSettings(PawnOwner), CapsuleInfo, bVaultTraceComplex);
}

FVIVaultInfo UVIPawnVaultComponent::ComputeVaultInfoFromResult_Implementation(const FVIVaultInfo& VaultResult) const
{
	FVIVaultInfo Result;

	// Cache properties
	const FVector& Up = PawnOwner->GetActorUpVector();
	const FVector Location = VaultResult.Location - Up * CapsuleInfo.HalfHeight;

	const bool bUseAdditionalHeight = (AdditionalVaultHeight > 0.f || AdditionalVaultHeightFalling > 0.f);
	const float AdditionalHeight = (bUseAdditionalHeight && PawnOwner->GetMovementComponent() && PawnOwner->GetMovementComponent()->IsFalling()) ? AdditionalVaultHeightFalling : AdditionalVaultHeight;
	const FVector AdditionalHeightOffset = (bUseAdditionalHeight) ? Up * AdditionalHeight : FVector::ZeroVector;

	// Compute
	Result.Location = Location + AdditionalHeightOffset;					// Where we are going on the ledge
	Result.Direction = VaultResult.Direction;								// Used by montage notify to face ledge
	Result.SetHeight(VaultResult.Height);									// Used to determine which animation set to use

	if (PawnOwner->IsLocallyControlled())
	{
		Result.RandomSeed = FMath::RandRange(0, 999);						// Used as a rand seed to randomize animations
	}

	return Result;
}

bool UVIPawnVaultComponent::ComputeShouldAutoVault_Implementation()
{
	if (!CapsuleInfo.IsValidCapsule())
	{
		return false;
	}

	const FVector& VaultDirection = IVIPawnInterface::Execute_GetVaultDirection(PawnOwner).GetSafeNormal();
	if (VaultDirection.IsNearlyZero())
	{
		// Do not have a usable vector
		return false;
	}

	SCOPE_CYCLE_COUNTER(STAT_VAULTAUTOVAULT);

	// Trace to see if a potentially vault-able object is ahead of us
	const FVITraceSettings& TraceSettings = IVIPawnInterface::Execute_GetVaultTraceSettings(PawnOwner);

	const float HeightOffset = CapsuleInfo.HalfHeight + TraceSettings.CollisionFloatHeight;
	const FVector& Up = PawnOwner->GetActorUpVector();
	const FVector BaseLoc = PawnOwner->GetActorLocation() - (Up * HeightOffset) - (Up * CapsuleInfo.Radius * 0.05f);

	const FVector TraceStart = BaseLoc + Up * ((TraceSettings.MaxLedgeHeight + TraceSettings.MinLedgeHeight) * 0.5f);
	const FVector TraceEnd = TraceStart + (VaultDirection * TraceSettings.ReachDistance);
	const float TraceHalfHeight = 1.f + ((TraceSettings.MaxLedgeHeight - TraceSettings.MinLedgeHeight) * 0.5f);

	const TArray<AActor*> TraceIgnore = { PawnOwner };

	FHitResult Hit(ForceInit);
	UVIBlueprintFunctionLibrary::CapsuleTraceSingleForObjects(
		PawnOwner,																// World Context
		TraceStart,																// Start
		TraceEnd,																// End
		PawnOwner->GetActorQuat(),												// Rotation
		CapsuleInfo.Radius,														// Radius
		TraceHalfHeight,														// HalfHeight
		TraceSettings.GetObjectTypes(),											// TraceChannels
		false,																	// bTraceComplex
		TraceIgnore,															// ActorsToIgnore
		false,																	// bDrawDebug (ForDuration)
		Hit,																	// OutHit
		false,																	// bIgnoreSelf
		FLinearColor::Red,														// TraceColor
		FLinearColor::Green,													// TraceHitColor
		1.f																		// DrawTime
	);

	return Hit.bBlockingHit;
}

FVICapsuleInfo UVIPawnVaultComponent::GetCapsuleInfo_Implementation() const
{
	// Always return the cached info if its valid, no need to do anything
	// User is expected to update CapsuleInfo themselves if the size changes
	if (CapsuleInfo.IsValidCapsule())
	{
		return CapsuleInfo;
	}
	else if (const ACharacter* const CharacterOwner = Cast<ACharacter>(PawnOwner))
	{
		// Automatically use the character's capsule if owner is a character
		if (CharacterOwner->GetCapsuleComponent())
		{
			return FVICapsuleInfo(CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius());
		}
		else
		{
			// Just to prevent a crash in some strange contexts, better to pick it up so it can be resolved
			ensure(false);
		}
	}
	else
	{
		// Pawns must override this and return information akin to a capsule
		UVIBlueprintFunctionLibrary::MessageLogError("UVIPawnVaultComponent::GetCapsuleInfo requires override for non-characters", true);
	}

	return CapsuleInfo;
}

bool UVIPawnVaultComponent::ComputeAntiCheatResult(const FVIVaultInfo& ClientVaultInfo) const
{
	if (!PawnOwner)
	{
		return true;
	}

	// AntiCheat disabled
	if (AntiCheatType == EVIAntiCheatType::VIACT_None)
	{
		return true;
	}

	// Standalone and listen server and AI and client doesn't need to test
	// Only need to test for remote players who sent info to server
	if (PawnOwner->IsLocallyControlled())
	{
		return true;
	}

	// If too out of sync, deny vault (this will cause client to de-sync as server will force vaulting to end and return to original location)
	switch (AntiCheatType)
	{
	case EVIAntiCheatType::VIACT_Enabled:
		{
			// Custom logic for expensive anti-cheat, server doing same checks as client and comparing the result
			const FVIVaultResult& ServerVaultResult = ComputeVault();
			if (!ServerVaultResult.bSuccess)
			{
				// User may want to use more lenient settings for authority in VIPawnInterface::GetVaultTraceSettings()
				return false;
			}

			// Convert trace to usable info
			const FVIVaultInfo ServerVaultInfo = ComputeVaultInfoFromResult(ServerVaultResult);

			if (!AntiCheatSettings.ComputeAntiCheat(ClientVaultInfo, ServerVaultInfo, PawnOwner))
			{
				return false;
			}
		}
		return true;
	case EVIAntiCheatType::VIACT_Custom:
		// Using custom AntiCheat override
		return ComputeCustomAntiCheat(ClientVaultInfo);
	case EVIAntiCheatType::VIACT_None:
	default:
		break;
	}

	return true;
}

bool FVIAntiCheatSettings::ComputeAntiCheat(const FVIVaultInfo& ClientInfo, const FVIVaultInfo& ServerInfo, const APawn* const Pawn) const
{
	// The results here are almost always nearly identical due to prediction unless de-syncing, lower tolerances can be used

	if (!Pawn)
	{
		return true;
	}

	// Location test (magnitude)
	if (LocationErrorThreshold > -1.f)
	{
		const float LocDiff = (ServerInfo.Location - ClientInfo.Location).Size();
		if (LocDiff >= LocationErrorThreshold)
		{
			UE_LOG(LogVaultItAntiCheat, Warning, TEXT("{ %s } failed anti-cheat test due to error in location comparison with diff %f"), *Pawn->GetName(), LocDiff);
			return false;
		}
	}

	// Direction test (dot product)
	if (DirectionErrorThreshold > -1.f)
	{
		const float DirDiff = (ServerInfo.Direction | ClientInfo.Direction);
		if (DirDiff < (1.f - DirectionErrorThreshold))
		{
			UE_LOG(LogVaultItAntiCheat, Warning, TEXT("{ %s } failed anti-cheat test due to error in direction comparison with diff %f"), *Pawn->GetName(), DirDiff);
			return false;
		}
	}

	// Height test (magnitude)
	if (HeightErrorThreshold > -1.f)
	{
		const float HeightDiff = (ServerInfo.Height - ClientInfo.Height);
		if (HeightDiff >= HeightErrorThreshold)
		{
			UE_LOG(LogVaultItAntiCheat, Warning, TEXT("{ %s } failed anti-cheat test due to error in height comparison with diff %f"), *Pawn->GetName(), HeightDiff);
			return false;
		}
	}

	return true;
}

bool UVIPawnVaultComponent::ComputeCustomAntiCheat_Implementation(const FVIVaultInfo& ClientVaultInfo) const
{
	UVIBlueprintFunctionLibrary::MessageLogError("UVIPawnVaultComponent::PassCustomAntiCheatAnalysis requires override and testing, otherwise will always return true", true);
	return true;
}

bool UVIPawnVaultComponent::PredictLandingLocation(FPredictProjectilePathResult& OutPredictResult, float HalfHeight, float Radius, float GravityZ) const
{
	return UVIBlueprintFunctionLibrary::PredictLandingLocation(OutPredictResult, PawnOwner, IVIPawnInterface::Execute_GetVaultTraceSettings(PawnOwner).GetObjectTypes(), HalfHeight, Radius, GravityZ, bVaultTraceComplex);
}
