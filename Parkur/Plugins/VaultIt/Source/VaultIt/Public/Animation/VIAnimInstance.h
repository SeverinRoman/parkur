// Copyright (c) 2019-2022 Drowning Dragons Limited. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "VIAnimationInterface.h"
#include "VITypes.h"
#include "VIAnimInstance.generated.h"

class AVICharacterBase;

/**
 * 
 */
UCLASS()
class VAULTIT_API UVIAnimInstance : public UAnimInstance, public IVIAnimationInterface
{
	GENERATED_BODY()
	
protected:
	/** Name of Right FBIK Bone */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FBIK)
	FName RHandName;

	/** Name of Left FBIK Bone */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FBIK)
	FName LHandName;

	/** FBIK Settings and Data */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FBIK)
	TArray<FVIBoneFBIKData> FBIK;

	/** Used by Anim Graph to determine right hand FBIK is used */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FBIK, meta = (DisplayName = "R Hand"))
	bool bRHand;

	/** Used by Anim Graph to determine left hand FBIK is used */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FBIK, meta = (DisplayName = "L Hand"))
	bool bLHand;

	/** Used by Anim Graph to determine both hand FBIK is used */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FBIK)
	bool bBothHand;

	/** Used by Anim Graph to determine right hand FBIK location */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FBIK, meta = (DisplayName = "R Hand Loc"))
	FVector RHandLoc;

	/** Used by Anim Graph to determine left hand FBIK location */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = FBIK, meta = (DisplayName = "L Hand Loc"))
	FVector LHandLoc;

	/** Is currently vaulting */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Vault)
	bool bIsVaulting;

protected:
	UPROPERTY(BlueprintReadWrite, Category = References)
	AVICharacterBase* Character;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = AnimGraph)
	bool bIsJumping;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = AnimGraph)
	bool bIsFalling;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = AnimGraph)
	float Speed;

public:
	UVIAnimInstance()
		: RHandName(TEXT("hand_r"))
		, LHandName(TEXT("hand_l"))
		, FBIK( { FVIBoneFBIKData(RHandName), FVIBoneFBIKData(LHandName) } )
	{}

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaTime) override;

protected:
	virtual void OnStartVault();
	virtual void OnStopVault();

	/** Called when starting vaulting */
	UFUNCTION(BlueprintImplementableEvent, Category = Vault, meta = (DisplayName = "On Start Vault"))
	void K2_OnStartVault();

	/** Called when stopping vaulting */
	UFUNCTION(BlueprintImplementableEvent, Category = Vault, meta = (DisplayName = "On Stop Vault"))
	void K2_OnStopVault();

public:
	/**
	 * Called by anim notify when a bone has its location updated for FBIK
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Vault)
	void SetBoneFBIK(const FName& BoneName, const FVector& BoneLocation, bool bEnabled);
};
