// Copyright (c) 2019-2022 Drowning Dragons Limited. All Rights Reserved.

#include "Pawn/VICharacterBase.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Pawn/VIPawnVaultComponent.h"
#include "VIMotionWarpingComponent.h"
#include "VIBlueprintFunctionLibrary.h"

void AVICharacterBase::BeginPlay()
{
	Super::BeginPlay();

	VaultComponent = IVIPawnInterface::Execute_GetPawnVaultComponent(this);
	MotionWarping = IVIPawnInterface::Execute_GetMotionWarpingComponent(this);
}

void AVICharacterBase::CheckJumpInput(float DeltaTime)
{
	const bool bIsVaulting = IsVaulting();

	// Server update simulated proxies with correct vaulting state
	if (GetLocalRole() == ROLE_Authority && GetNetMode() != NM_Standalone)
	{
		bRepIsVaulting = bIsVaulting;
	}

	// Try to vault from local input
	if (IsLocallyControlled() && VaultComponent)
	{
		// Disable jump if vaulting
		if (VaultComponent->bPressedVault)
		{
			bPressedJump = false;
		}

		// Possibly execute vault
		if (GetCharacterMovement())
		{
			VaultComponent->CheckVaultInput(DeltaTime, GetCharacterMovement()->MovementMode);
		}
		else
		{
			VaultComponent->CheckVaultInput(DeltaTime);
		}
	}

	// Pick up changes in vaulting state to change movement mode
	// to something other than flying (required for root motion on Z)
	if (bWasVaulting && !bIsVaulting)
	{
		StopVaultAbility();
	}

	// Call super so we actually jump if we're meant to
	Super::CheckJumpInput(DeltaTime);

	// Cache end of frame
	bWasVaulting = bIsVaulting;
}

void AVICharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AVICharacterBase, bRepIsVaulting, COND_SimulatedOnly);
	DOREPLIFETIME_CONDITION(AVICharacterBase, RepMotionMatch, COND_SimulatedOnly);
}

void AVICharacterBase::Jump()
{
	// If missing critical components then jump and exit
	if (!VaultComponent || !GetCharacterMovement())
	{
		Super::Jump();
		return;
	}

	// Either jump or vault, determined by VaultComponent::EVIJumpKeyPriority
	if (VaultComponent->Jump(GetCharacterMovement()->GetGravityZ(), CanJump(), GetCharacterMovement()->IsFalling()))
	{
		// Jump normally
		Super::Jump();
	}
	else
	{
		// Jump key essentially presses the vault input
		VaultComponent->Vault();
	}
}

void AVICharacterBase::StopJumping()
{
	Super::StopJumping();

	// Release vault input if the jump key pressed vault instead
	if (VaultComponent)
	{
		VaultComponent->StopJumping();
	}
}

void AVICharacterBase::StartVaultAbility_Implementation()
{
	// Called by GA_Vault
	// Need to be in flying mode to have root motion on Z axis
	if (GetCharacterMovement() && GetLocalRole() > ROLE_SimulatedProxy)
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	}
}

void AVICharacterBase::StopVaultAbility()
{
	// Called by CheckJumpInput()
	// Exiting flying mode
	// This may put is straight into falling if we aren't properly grounded, which is fine
	if (GetCharacterMovement() && GetLocalRole() > ROLE_SimulatedProxy)
	{
		GetCharacterMovement()->SetMovementMode(GetCharacterMovement()->GetGroundMovementMode());
	}

	OnStopVaultAbility();
}

void AVICharacterBase::OnRep_MotionMatch()
{
	UE_LOG(LogTemp, Log, TEXT("MATCH"));
	// Simulated proxies update their sync points here, sent from the server during GA_Vault
	MotionWarping->AddOrUpdateSyncPoint(TEXT("VaultSyncPoint"), FVIMotionWarpingSyncPoint(RepMotionMatch.Location, RepMotionMatch.Direction.ToOrientationQuat()));
}

bool AVICharacterBase::IsVaulting() const
{
	// Simulated proxies use the value provided by server
	if (GetLocalRole() == ROLE_SimulatedProxy)
	{
		return bRepIsVaulting;
	}

	// Local and authority uses gameplay tags for a predicted result
	if (VaultComponent)
	{
		return VaultComponent->IsVaulting();
	}

	return false;
}

// *********************************************** //
// ******** Begin Pawn Vaulting Interface ******** //
// *********************************************** //

UVIPawnVaultComponent* AVICharacterBase::GetPawnVaultComponent_Implementation() const
{
	// You need to override this
	UVIBlueprintFunctionLibrary::MessageLogError(FString::Printf(TEXT("AVICharacterBase::GetPawnVaultComponent not implemented for { %s }. Cannot Vault."), *GetName()));
	return nullptr;
}

UVIMotionWarpingComponent* AVICharacterBase::GetMotionWarpingComponent_Implementation() const
{
	// You need to override this
	UVIBlueprintFunctionLibrary::MessageLogError(FString::Printf(TEXT("AVICharacterBase::GetMotionWarpingComponent not implemented for { %s }. Cannot Vault."), *GetName()));
	return nullptr;
}

FVIAnimSet AVICharacterBase::GetVaultAnimSet_Implementation() const
{
	// You need to override this
	UVIBlueprintFunctionLibrary::MessageLogError(FString::Printf(TEXT("AVICharacterBase::GetVaultAnimSet not implemented for { %s }. Cannot Vault."), *GetName()));
	return FVIAnimSet();
}

FVITraceSettings AVICharacterBase::GetVaultTraceSettings_Implementation() const
{
	// You need to override this
	UVIBlueprintFunctionLibrary::MessageLogError(FString::Printf(TEXT("AVICharacterBase::GetVaultTraceSettings not implemented for { %s }. Using default trace settings."), *GetName()), false);
	return FVITraceSettings();
}

FVector AVICharacterBase::GetVaultDirection_Implementation() const
{
	// Use input vector if available
	if (GetCharacterMovement() && !GetCharacterMovement()->GetCurrentAcceleration().IsNearlyZero())
	{
		return GetCharacterMovement()->GetCurrentAcceleration();
	}

	// Use character facing direction if not providing input
	return GetActorForwardVector();
}

bool AVICharacterBase::CanVault_Implementation() const
{
	// Vaulting must finish before starting another vault attempt
	if (IsVaulting())
	{
		return false;
	}

	// Invalid components
	if (!VaultComponent || !GetCharacterMovement())
	{
		return false;
	}

	// Animation instance is required to play vault montage
	if (!GetMesh() || !GetMesh()->GetAnimInstance())
	{
		return false;
	}

	// Authority not initialized (this isn't set on clients)
	if (HasAuthority() && !VaultComponent->bVaultAbilityInitialized)
	{
		return false;
	}

	// Exit if character is in a state they cannot vault from
	if (GetCharacterMovement()->IsMovingOnGround() || GetCharacterMovement()->IsFalling() || GetCharacterMovement()->IsSwimming())
	{
		if (GetCharacterMovement()->IsMovingOnGround() && !VaultComponent->bCanVaultFromGround)
		{
			return false;
		}

		if (GetCharacterMovement()->IsFalling() && !VaultComponent->bCanVaultFromFalling)
		{
			return false;
		}

		if (GetCharacterMovement()->IsSwimming() && !VaultComponent->bCanVaultFromSwimming)
		{
			return false;
		}
	}
	else
	{
		return false;
	}

	// Can't vault while crouching
	if (!VaultComponent->bCanVaultFromCrouching && GetCharacterMovement()->IsCrouching())
	{
		return false;
	}

	// Passed all conditions
	return true;
}

void AVICharacterBase::OnLocalPlayerVault_Implementation(const FVector& Location, const FVector& Direction)
{
	// LocalPlayer just stores the data in the same place for convenience, ease of use, memory reduction, etc
	RepMotionMatch = FVIRepMotionMatch(Location, Direction);
}

void AVICharacterBase::GetVaultLocationAndDirection_Implementation(FVector& OutLocation, FVector& OutDirection) const
{
	// Because LocalPlayer stores in the same place, no need for any testing as they all use RepMotionMatch to store this

	// This is only currently used for FBIK tracing
	OutLocation = RepMotionMatch.Location;
	OutDirection = RepMotionMatch.Direction;
}

void AVICharacterBase::ReplicateMotionMatch_Implementation(const FVIRepMotionMatch& MotionMatch)
{
	// GA_Vault has directed server to update it's RepMotionMatch property so that it will
	// be replicated to simulated proxies with 1 decimal point of precision (net quantization)
	RepMotionMatch = MotionMatch;
}

bool AVICharacterBase::IsWalkable_Implementation(const FHitResult& HitResult) const
{
	// Surface we hit can be walked on or not
	return GetCharacterMovement() && GetCharacterMovement()->IsWalkable(HitResult);
}

bool AVICharacterBase::CanAutoVaultInCustomMovementMode_Implementation() const
{
	return true;

	// Example usage commented out

	/*
	
	if (GetCharacterMovement())
	{
		switch (GetCharacterMovement()->CustomMovementMode)
		{
		case 0:
			return false;
		case 1:  // Some example custom mode where auto vault can work
			return true;
		case 2:
			return false;
		default:
			return true;
		}
	}

	*/
}

// *********************************************** //
// ********* End Pawn Vaulting Interface ********* //
// *********************************************** //