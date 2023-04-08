// Copyright (c) 2019-2022 Drowning Dragons Limited. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "VIAbilitySystemComponent.generated.h"

class USkeletalMeshComponent;

/**
 * Data about montages that were played locally (all montages in case of server. Predictive montages in case of client). Never replicated directly.
 */
USTRUCT()
struct VAULTIT_API FVIGameplayAbilityLocalAnimMontageForMesh
{
	GENERATED_BODY();

public:
	UPROPERTY()
	USkeletalMeshComponent* Mesh;

	UPROPERTY()
	FGameplayAbilityLocalAnimMontage LocalMontageInfo;

	FVIGameplayAbilityLocalAnimMontageForMesh() 
		: Mesh(nullptr)
	{
	}

	FVIGameplayAbilityLocalAnimMontageForMesh(USkeletalMeshComponent* InMesh)
		: Mesh(InMesh)
	{
	}

	FVIGameplayAbilityLocalAnimMontageForMesh(USkeletalMeshComponent* InMesh, FGameplayAbilityLocalAnimMontage& InLocalMontageInfo)
		: Mesh(InMesh)
		, LocalMontageInfo(InLocalMontageInfo)
	{
	}
};

/**
* Data about montages that is replicated to simulated clients.
* Based on GASShooter by Dan Kestranek
*/
USTRUCT()
struct VAULTIT_API FVIGameplayAbilityRepAnimMontageForMesh
{
	GENERATED_BODY();

public:
	UPROPERTY()
	USkeletalMeshComponent* Mesh;

	UPROPERTY()
	FGameplayAbilityRepAnimMontage RepMontageInfo;

	FVIGameplayAbilityRepAnimMontageForMesh() 
		: Mesh(nullptr)
	{
	}

	FVIGameplayAbilityRepAnimMontageForMesh(USkeletalMeshComponent* InMesh)
		: Mesh(InMesh)
	{
	}
};

/**
 * 
 */
UCLASS()
class VAULTIT_API UVIAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
	
protected:
	/**
	 * Montages instigated locally
	 * All if server, predicted if client, replicated if simulated
	 */
	UPROPERTY()
	TArray<FVIGameplayAbilityLocalAnimMontageForMesh> LocalAnimMontageInfoForMeshes;

	/**
	 * Replicates montage info to simulated clients
	 */
	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedAnimMontageForMesh)
	TArray<FVIGameplayAbilityRepAnimMontageForMesh> RepAnimMontageInfoForMeshes;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool GetShouldTick() const override;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;

	virtual void NotifyAbilityEnded(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, bool bWasCancelled) override;

	/** Plays montage, handles replication and prediction based on passed in ability or activation info */
	virtual float PlayMontageForMesh(UGameplayAbility* AnimatingAbility, class USkeletalMeshComponent* InMesh, FGameplayAbilityActivationInfo ActivationInfo, UAnimMontage* Montage, float InPlayRate, FName StartSectionName = NAME_None, bool bReplicateMontage = true);

	/** Plays a montage without updating replication or prediction structures. Used by simulated proxies when replication tells them to play a montage. */
	virtual float PlayMontageSimulatedForMesh(USkeletalMeshComponent* InMesh, UAnimMontage* Montage, float InPlayRate, FName StartSectionName = NAME_None);

	/** Returns the current animating ability */
	UGameplayAbility* GetAnimatingAbilityFromAnyMesh();

	// Returns the montage that is playing for the mesh
	UAnimMontage* GetCurrentMontageForMesh(USkeletalMeshComponent* InMesh);

	/** Stops whatever montage is currently playing. Expectation is caller should only be stopping it if they are the current animating ability (or have good reason not to check) */
	virtual void CurrentMontageStopForMesh(USkeletalMeshComponent* InMesh, float OverrideBlendOutTime = -1.0f);

	/** Clear the animating ability that is passed in, if it's still currently animating */
	virtual void ClearAnimatingAbilityForAllMeshes(UGameplayAbility* Ability);

protected:
	/** Copy LocalAnimMontageInfo into RepAnimMontageInfo */
	void AnimMontage_UpdateReplicatedDataForMesh(USkeletalMeshComponent* InMesh);
	void AnimMontage_UpdateReplicatedDataForMesh(FVIGameplayAbilityRepAnimMontageForMesh& OutRepAnimMontageInfo);

	UFUNCTION()
	virtual void OnRep_ReplicatedAnimMontageForMesh();

	/** Finds existing FGameplayAbilityLocalAnimMontageForMesh for the mesh or creates one if it doesn't exist */
	FVIGameplayAbilityLocalAnimMontageForMesh& GetLocalAnimMontageInfoForMesh(USkeletalMeshComponent* InMesh);

	/** Finds existing FGameplayAbilityRepAnimMontageForMesh for the mesh or creates one if it doesn't exist */
	FVIGameplayAbilityRepAnimMontageForMesh& GetGameplayAbilityRepAnimMontageForMesh(USkeletalMeshComponent* InMesh);

	/** Called when a prediction key that played a montage is rejected */
	void OnPredictiveMontageRejectedForMesh(USkeletalMeshComponent* InMesh, UAnimMontage* PredictiveMontage);

	/**
	 * @return True if we are ready to handle replicated montage information 
	 * Not used by default, may be overridden
	 */
	virtual bool IsReadyForReplicatedMontageForMesh() { return true; }
};
