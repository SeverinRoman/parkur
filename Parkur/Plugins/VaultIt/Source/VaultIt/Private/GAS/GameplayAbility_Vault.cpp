// Copyright (c) 2019-2022 Drowning Dragons Limited. All Rights Reserved.

#include "GAS/GameplayAbility_Vault.h"

UGameplayAbility_Vault::UGameplayAbility_Vault()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	bServerRespectsRemoteAbilityCancellation = false;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnlyTermination;
}
