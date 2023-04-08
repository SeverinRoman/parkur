// Copyright (c) 2019-2022 Drowning Dragons Limited. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VITypes.h"
#include "VIPawnInterface.generated.h"

class UVIMotionWarpingComponent;
class UVIPawnVaultComponent;

UINTERFACE(BlueprintType)
class VAULTIT_API UVIPawnInterface : public UInterface
{
	GENERATED_BODY()
};

class VAULTIT_API IVIPawnInterface
{
	GENERATED_BODY()

public:
	/**
	 * VIPawnVaultComponent should either be derived as a blueprint or an
	 * existing blueprint should be copied and added to your pawn or character
	 * and returned by this event
	 * 
	 * Ideally it should also be set and cached on the character for cheaper access
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Vault)
	UVIPawnVaultComponent* GetPawnVaultComponent() const;

	/**
	 * Simply return the motion warping component that has been added to
	 * your pawn or character
	 * 
	 * Ideally it should also be set and cached on the character for cheaper access
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Vault)
	UVIMotionWarpingComponent* GetMotionWarpingComponent() const;

	/**
	 * This is the mesh that players the vaulting montage
	 * In most cases it is the character's mesh but it can be whichever
	 * is capable of driving root motion
	 * 
	 * If you have first and third person, it is advisable to use third
	 * person mesh and have the first person animation line up both
	 * cosmetically and in terms of length
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Vault)
	USkeletalMeshComponent* GetMeshForVaultMontage() const;

	/**
	 * Provide animation set used when vaulting
	 * You can use CharacterMovementComponent's movement mode such as
	 * Falling, Swimming, etc to provide different results
	 *
	 * The runtime change in vault animation must come through some form of
	 * prediction so the server and client are never out of sync
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Vault)
	FVIAnimSet GetVaultAnimSet() const;

	/**
	 * Provide trace settings used when vaulting
	 * You can use CharacterMovementComponent's movement mode such as
	 * Falling, Swimming, etc to provide different results
	 *
	 * If you use a more extreme anti-cheat that requires the server to do
	 * full vaulting checks then the runtime change in trace settings must
	 * come through some form of prediction so the server and client are never 
	 * out of sync
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Vault)
	FVITraceSettings GetVaultTraceSettings() const;

	/**
	 * The vector used to determine the direction to vault in
	 * There is no need to normalize this vector; it will just end up being
	 * done twice
	 * 
	 * The most responsive option is to use acceleration (input) if available
	 * and otherwise use the character's forward direction
	 * 
	 * You could also use the velocity direction, however that could frustrate
	 * players because it may ignore or go against their intentions but
	 * should provide a much more realistic result
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Vault)
	FVector GetVaultDirection() const;

	/**
	 * Logic to determine if we are currently able to vault
	 * UVIPawnVaultComponent::IsVaulting() should be compatible with GAS
	 * as it is based on state gameplay tags
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Vault)
	bool CanVault() const;

	/**
	 * Called by GA_Vault when the vault ability starts
	 * For characters, they need to have their movement components placed
	 * into flying mode for root motion to work on Z axis (a natural requirement
	 * for vaulting)
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Vault)
	void StartVaultAbility();

	/**
	 * Called when the local player starts vaulting to allow them to cache
	 * the Location and Direction for other purposes, in particular, FBIK
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Vault)
	void OnLocalPlayerVault(const FVector& Location, const FVector& Direction);

	/**
	 * Return the cached location and direction for vaulting
	 * On local player this is the data stored from OnLocalPlayerVault()
	 * On server and simulated proxy this is the data stored from ReplicateMotionMatch()
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Vault)
	void GetVaultLocationAndDirection(FVector& OutLocation, FVector& OutDirection) const;

	/**
	 * Called by GA_Vault to notify server that it should update a predicted
	 * property that simulated clients receive to set their motion matching
	 * sync point
	 * 
	 * Net quantized to 1 decimal place
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Vault)
	void ReplicateMotionMatch(const FVIRepMotionMatch& MotionMatch);

	/**
	 * Determine if the surface provided by the hit result is walkable or not
	 * For characters this is UCharacterMovementComponent::IsWalkable()
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Vault)
	bool IsWalkable(const FHitResult& HitResult) const;

	/**
	 * This is only used when AutoVaultSettings utilizes "Custom" and the Pawn is in custom movement mode
	 * This is where you check against the custom movement mode it is currently in to determine if it can auto vault in this mode
	 */
	UFUNCTION(BlueprintNativeEvent, Category = Vault)
	bool CanAutoVaultInCustomMovementMode() const;
};