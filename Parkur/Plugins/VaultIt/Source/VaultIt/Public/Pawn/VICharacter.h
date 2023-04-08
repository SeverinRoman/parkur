// Copyright (c) 2019-2022 Drowning Dragons Limited. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VICharacterAbilityBase.h"
#include "VICharacter.generated.h"

class AController;


/**
 * This is a class that is fully setup for vaulting
 * VIPawnComponent needs additional setup as it has per-project settings (GameplayTags)
 * VaultTraceSettings also needs additional setup as it has per-project settings (Collision Profile)
 */
UCLASS()
class VAULTIT_API AVICharacter : public AVICharacterAbilityBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, Category = Vault)
	FVIAnimSet VaultAnimSet;

	UPROPERTY(EditDefaultsOnly, Category = Vault)
	FVITraceSettings VaultTraceSettings;

public:
	AVICharacter(const FObjectInitializer& OI);

	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;

	virtual UVIPawnVaultComponent* GetPawnVaultComponent_Implementation() const override { return VaultComponent; }
	virtual UVIMotionWarpingComponent* GetMotionWarpingComponent_Implementation() const override { return MotionWarping; }

	virtual FVIAnimSet GetVaultAnimSet_Implementation() const override { return VaultAnimSet; }
	virtual FVITraceSettings GetVaultTraceSettings_Implementation() const override { return VaultTraceSettings; }
};
