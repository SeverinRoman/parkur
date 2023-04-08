// Copyright (c) 2019-2022 Drowning Dragons Limited. All Rights Reserved.

#include "Pawn/VICharacterAbilityBase.h"
#include "GAS/VIAbilitySystemComponent.h"

AVICharacterAbilityBase::AVICharacterAbilityBase(const FObjectInitializer& OI)
	: Super(OI)
{
	AbilitySystem = CreateDefaultSubobject<UVIAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	AbilitySystemReplicationMode = (EVIGameplayEffectReplicationMode)(uint8)AbilitySystem->ReplicationMode;
}

#if WITH_EDITOR
void AVICharacterAbilityBase::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName().IsEqual(GET_MEMBER_NAME_CHECKED(AVICharacterAbilityBase, AbilitySystemReplicationMode)))
	{
		AbilitySystem->SetReplicationMode((EGameplayEffectReplicationMode)(uint8)AbilitySystemReplicationMode);
	}
}
#endif  // WITH_EDITOR

UAbilitySystemComponent* AVICharacterAbilityBase::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}
