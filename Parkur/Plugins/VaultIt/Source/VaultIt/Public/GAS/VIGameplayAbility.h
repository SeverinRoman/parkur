// Copyright (c) 2019-2022 Drowning Dragons Limited. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "VIGameplayAbility.generated.h"

class USkeletalMeshComponent;
class UAnimMontage;

USTRUCT()
struct VAULTIT_API FVIAbilityMeshMontage
{
	GENERATED_BODY()

public:
	UPROPERTY()
	USkeletalMeshComponent* Mesh;

	UPROPERTY()
	UAnimMontage* Montage;

	FVIAbilityMeshMontage() 
		: Mesh(nullptr)
		, Montage(nullptr)
	{}

	FVIAbilityMeshMontage(USkeletalMeshComponent* InMesh, UAnimMontage* InMontage)
		: Mesh(InMesh)
		, Montage(InMontage)
	{}
};

/**
 * 
 */
UCLASS()
class VAULTIT_API UVIGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()
	
protected:
	/** Active montages being played by this ability */
	UPROPERTY()
	TArray<FVIAbilityMeshMontage> CurrentAbilityMeshMontages;

public:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif  // WITH_EDITOR

	/** Call to set the current montage from a montage task. Set to allow hooking up montage events to ability events */
	virtual void SetCurrentMontageForMesh(USkeletalMeshComponent* InMesh, UAnimMontage* InCurrentMontage);

protected:
	bool FindAbillityMeshMontage(const USkeletalMeshComponent* InMesh, FVIAbilityMeshMontage& InAbilityMontage);

public:
	/** Returns the current prediction key and if it's valid for more prediction. Used for debugging ability prediction windows */
	UFUNCTION(BlueprintPure, Category = "Ability")
	virtual FString GetCurrentPredictionKeyStatus() const;

	/** Returns the current prediction key and if it's valid for more prediction */
	UFUNCTION(BlueprintPure, Category = "Ability")
	virtual bool IsPredictionKeyValidForMorePrediction() const;
};
