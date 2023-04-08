// Copyright (c) 2019-2022 Drowning Dragons Limited. All Rights Reserved.

#include "GAS/VIGameplayAbility.h"
#include "Logging/MessageLog.h"
#include "AbilitySystemComponent.h"

#if WITH_EDITOR
void UVIGameplayAbility::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName().IsEqual(TEXT("InstancingPolicy")))
	{
		if (InstancingPolicy == EGameplayAbilityInstancingPolicy::NonInstanced)
		{
			InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

			FMessageLog MsgLog("AssetCheck");

			MsgLog.Error(FText::FromString("VIGameplayAbility must be instanced, resetting to default automatically"));
			MsgLog.Open(EMessageSeverity::Error);
		}
	}
}
#endif  // WITH_EDITOR

void UVIGameplayAbility::SetCurrentMontageForMesh(USkeletalMeshComponent* InMesh, UAnimMontage* InCurrentMontage)
{
	ensure(IsInstantiated());

	FVIAbilityMeshMontage AbilityMeshMontage;
	if (FindAbillityMeshMontage(InMesh, AbilityMeshMontage))
	{
		AbilityMeshMontage.Montage = InCurrentMontage;
	}
	else
	{
		CurrentAbilityMeshMontages.Add(FVIAbilityMeshMontage(InMesh, InCurrentMontage));
	}
}

bool UVIGameplayAbility::FindAbillityMeshMontage(const USkeletalMeshComponent* InMesh, FVIAbilityMeshMontage& InAbilityMeshMontage)
{
	for (const FVIAbilityMeshMontage& MeshMontage : CurrentAbilityMeshMontages)
	{
		if (MeshMontage.Mesh == InMesh)
		{
			InAbilityMeshMontage = MeshMontage;
			return true;
		}
	}

	return false;
}

FString UVIGameplayAbility::GetCurrentPredictionKeyStatus() const
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	return ASC->ScopedPredictionKey.ToString() + " is valid for more prediction: " + (ASC->ScopedPredictionKey.IsValidForMorePrediction() ? TEXT("true") : TEXT("false"));
}

bool UVIGameplayAbility::IsPredictionKeyValidForMorePrediction() const
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	return ASC->ScopedPredictionKey.IsValidForMorePrediction();
}
