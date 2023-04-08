// Copyright (c) 2019-2022 Drowning Dragons Limited. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/VIAnimInstance.h"
#include "VIAnimInstanceFP.generated.h"

/**
 * 
 */
UCLASS()
class VAULTIT_API UVIAnimInstanceFP : public UVIAnimInstance
{
	GENERATED_BODY()
	
public:
	/**
	 * Key: ThirdPersonMontage
	 * Value: FirstPersonMontage that is played corresponding to the ThirdPersonMontage
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Vault)
	TMap<UAnimMontage*, UAnimMontage*> MontageLinkMap;

protected:
	UPROPERTY()
	UAnimMontage* MontageToPlay;

	UPROPERTY()
	float VaultBlendOutTime;

protected:
	bool CanPlayFPMontage() const;

public:
	virtual void OnStartVault() override;
	virtual void OnStopVault() override;
};
