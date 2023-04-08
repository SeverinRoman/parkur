// Copyright (c) 2019-2022 Drowning Dragons Limited. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VIAnimationInterface.generated.h"

/**
 * Used by AnimInstance by notify to update FBIK
 */
UINTERFACE(BlueprintType)
class VAULTIT_API UVIAnimationInterface : public UInterface
{
	GENERATED_BODY()
};

class VAULTIT_API IVIAnimationInterface
{
	GENERATED_BODY()

public:
	/**
	 * Called by anim notify when a bone has its location updated for FBIK
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Vault)
	void SetBoneFBIK(const FName& BoneName, const FVector& BoneLocation, bool bEnabled);
};