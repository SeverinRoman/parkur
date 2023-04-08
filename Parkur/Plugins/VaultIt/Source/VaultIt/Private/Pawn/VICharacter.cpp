// Copyright (c) 2019-2022 Drowning Dragons Limited. All Rights Reserved.

#include "Pawn/VICharacter.h"
#include "GAS/VIAbilitySystemComponent.h"
#include "Pawn/VIPawnVaultComponent.h"
#include "VIMotionWarpingComponent.h"

AVICharacter::AVICharacter(const FObjectInitializer& OI)
	: Super(OI)
{
	VaultComponent = CreateDefaultSubobject<UVIPawnVaultComponent>(TEXT("PawnVaulting"));
	MotionWarping = CreateDefaultSubobject<UVIMotionWarpingComponent>(TEXT("MotionWarping"));
}

void AVICharacter::BeginPlay()
{
	Super::BeginPlay();

	// Init simulated proxy
	if (AbilitySystem && GetLocalRole() == ROLE_SimulatedProxy)
	{
		// Will never have a valid controller
		AbilitySystem->InitAbilityActorInfo(this, this);
	}
}

void AVICharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Init authority/standalone
	if (AbilitySystem)
	{
		AbilitySystem->InitAbilityActorInfo(this, this);
	}
}

void AVICharacter::OnRep_Controller()
{
	Super::OnRep_Controller();

	// Init local client
	if (AbilitySystem)
	{
		AbilitySystem->InitAbilityActorInfo(this, this);
	}
}
