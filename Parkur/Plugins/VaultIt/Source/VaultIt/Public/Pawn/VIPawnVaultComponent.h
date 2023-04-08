// Copyright (c) 2019-2022 Drowning Dragons Limited. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "VITypes.h"
#include "VIPawnVaultComponent.generated.h"

class APawn;
class UGameplayAbility;
struct FPredictProjectilePathResult;

UENUM(BlueprintType)
enum class EVIVaultInputRelease : uint8
{
	VIR_Always							UMETA(DisplayName = "Always", ToolTip = "Always release vault input after a single test; recommended setting for performance"),
	VIR_OnSuccess						UMETA(DisplayName = "On Success", ToolTip = "Always release vault input after successfully vaulting; can be expensive computationally"),
	VIR_Never							UMETA(DisplayName = "Never", ToolTip = "Never release vault input unless the player does so; can be expensive computationally"),
};

/**
 * Used to set which movement modes auto vault works with
 */
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EVIAutoVault : uint8
{
	VIAV_None			= 0					UMETA(Hidden),
	VIAV_Walking		= 1 << 1			UMETA(DisplayName = "Walking"),
	VIAV_Falling		= 1 << 2			UMETA(DisplayName = "Falling"),
	VIAV_Swimming		= 1 << 3			UMETA(DisplayName = "Swimming"),
	VIAV_Flying			= 1 << 4			UMETA(DisplayName = "Flying"),
	VIAV_Custom			= 1 << 5			UMETA(DisplayName = "Custom", ToolTip = "Make sure to properly implement VIPawnInterface::CanAutoVaultInCustomMovementMode"),
};
ENUM_CLASS_FLAGS(EVIAutoVault);

UENUM(BlueprintType)
enum class EVIAntiCheatType : uint8
{
	VIACT_None							UMETA(DisplayName = "None", ToolTip = "No Anti-Cheat, always trust client. Suitable for co-op and non-competitive games"),
	VIACT_Enabled						UMETA(DisplayName = "Enabled", ToolTip = "Competitive anti-cheat, avoid using with high player count. Does full vault checks on server and offers tolerances within which to compare the client's data. If tolerances are too tight may de-sync the client and reject the vault"),
	VIACT_Custom						UMETA(DisplayName = "Custom", ToolTip = "Override ComputeCustomAntiCheat on the PawnVaultComponent to define behaviour that server verifies"),
};

/**
 * Performs a full vault check to compare to client's (can get expensive)
 * Should only be used in competitive environments without many players that vault
 * This check is never needed for AI
 */
USTRUCT(BlueprintType)
struct FVIAntiCheatSettings
{
	GENERATED_BODY()

	FVIAntiCheatSettings()
		: LocationErrorThreshold(10.f)
		, DirectionErrorThreshold(0.2f)
		, HeightErrorThreshold(4.f)
	{}

	virtual ~FVIAntiCheatSettings() = default;

	/**
	 * How far apart in unreal units the client and server can be with their vault location
	 * This check doesn't work for vaulting over objects below half height
	 * Set to -1 to disable check
	 * 
	 * Remember to allow for player collisions, packet loss, etc. that could prevent vaulting when not cheating
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Vault|AntiCheat", meta = (ClampMin = "-1", UIMin = "-1"))
	float LocationErrorThreshold;

	/**
	 * How far apart the client and server can be with their vault direction
	 * This check works in a range of -1 (opposite) to 1 (completely identical)
	 * Set to -1 to disable check
	 *
	 * Remember to allow for player collisions, packet loss, etc. that could prevent vaulting when not cheating
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Vault|AntiCheat", meta = (ClampMin = "-1", UIMin = "-1"))
	float DirectionErrorThreshold;
	
	/**
	 * How far apart in unreal units the client and server can be with their vault height
	 * Set to -1 to disable check
	 *
	 * Remember to allow for player collisions, packet loss, etc. that could prevent vaulting when not cheating
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Vault|AntiCheat", meta = (ClampMin = "-1", UIMin = "-1"))
	float HeightErrorThreshold;

	/** The results here are almost always identical due to prediction if network conditions are ideal (latency is OK, but packet loss or collisions can cause issues), low tolerances can be used */
	virtual bool ComputeAntiCheat(const FVIVaultInfo& ClientInfo, const FVIVaultInfo& ServerInfo, const APawn* const Pawn) const;
};

/**
 * Responsible for all vaulting logic that doesn't belong in a character or pawn
 * Owner must implement IVIPawnInterface
 * Owner must override most functions in IVIPawnInterface
 * Owner must implement IAbilitySystemInterface
 * Owner must have an AbilitySystemComponent
 * 
 * If owner is a pawn, you will need to create a blueprint and override GetCapsuleInfo() (read it's tooltip, it's important)
 */
UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent, DisplayName = "PawnVaultComponent"))
class VAULTIT_API UVIPawnVaultComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/**
	 * The GA_Vault GameplayAbility responsible for the action itself
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Ability")
	TSubclassOf<UGameplayAbility> VaultAbility;

	/** Tag used to initiate vaulting ability */
	UPROPERTY(EditDefaultsOnly, Category = "Ability")
	FGameplayTag VaultAbilityTag;

	/** Tag used to check if vaulting is active */
	UPROPERTY(EditDefaultsOnly, Category = "Ability")
	FGameplayTag VaultStateTag;

	/** Tag used to check if vaulting is active */
	UPROPERTY(EditDefaultsOnly, Category = "Ability")
	FGameplayTag VaultRemovalTag;

	/** 
	 * How to handle releasing of vault input
	 * Note that jump key will always auto release, this only applies to holding down vault key
	 * Allowing player to continue holding vault key as opposed to doing single checks each press
	 * can be computationally expensive as it does multiple traces every render tick
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Vault)
	EVIVaultInputRelease AutoReleaseVaultInput;

	/**
	 * What to do when pressing the jump key
	 * Disable Vault from Jump: Jump key cannot vault
	 * Select Highest Point: Checks ahead to see if jump can reach as high as vault, and will vault if it can go higher
	 * Always Vault if Possible: Will always vault if it is possible to do so, otherwise jump
	 * Only Vault from Air: Only vault if in air and jump key wont otherwise do anything (eg. double jump)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Vault)
	EVIJumpKeyPriority JumpKeyPriority;

	/**
	 * If true, will trace geometry instead of collision mesh when looking for vaulting surface
	 * This is helpful if using a map that wasn't setup with collisions that work well with vaulting and you just want something usable
	 * But a production ready game should probably disable this
	 * 
	 * This could result in vaulting onto something the character cannot actually move onto, as character movement does not use complex collision
	 */
	bool bVaultTraceComplex;

	/** 
	 * If not using root motion animations using only motion warping may fail to reach the ledge,
	 * providing extra height will fix this but requires more blending
	 * 
	 * When using root motion animations you should leave this at 0
	 *
	 * Only works when vaulting onto a surface higher than us
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Vault)
	float AdditionalVaultHeight;

	/** 
	 * Used instead of AdditionalVaultHeight if falling
	 * If not using root motion animations using only motion warping may fail to reach the ledge,
	 * providing extra height will fix this but requires more blending
	 * 
	 * When using root motion animations you should leave this at 0
	 *
	 * Only works when vaulting onto a surface higher than us
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Vault)
	float AdditionalVaultHeightFalling;

	/**
	 * Vault is allowed to occur when on ground
	 * The component itself does not use this setting, it is available for use by the owner when checking CanVault()
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Vault)
	bool bCanVaultFromGround;

	/**
	 * Vault is allowed to occur when in falling state
	 * The component itself does not use this setting, it is available for use by the owner when checking CanVault()
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Vault)
	bool bCanVaultFromFalling;

	/**
	 * Vault is allowed to occur when in swimming state
	 * The component itself does not use this setting, it is available for use by the owner when checking CanVault()
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Vault)
	bool bCanVaultFromSwimming;

	/**
	 * WARNING: It is up to you to handle the logic for crouching, VaultIt does not support this by default
	 * Vault is allowed to occur when crouching
	 * The component itself does not use this setting, it is available for use by the owner when checking CanVault()
	 * Crouching has a different capsule size, you will either need to update CapsuleInfo or stop crouching when vaulting starts
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Vault)
	bool bCanVaultFromCrouching;

	/**
	 * Optional auto vaulting
	 * This can be expensive as it is performed on tick (CheckVaultInput)
	 * However, it is not a full vault test, it only tests if something is in our way
	 *
	 * AI requires this to vault
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (Bitmask, BitmaskEnum = "EVIAutoVault"), Category = Vault)
	uint8 AutoVaultStates;

	/**
	 * Used to skip AutoVault checks for AutoVaultFrameSkip amount of ticks
	 * If set to 0, will always check on tick
	 * If set to 1, will check every second tick
	 * If set to 2, will check every third tick
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Vault)
	uint8 AutoVaultCheckSkip;

	/**
	 * Corrections from anti-cheat tend to be aggressive and cause rubber banding at most latencies, so be forgiving where possible
	 * Because vaulting is predicted there shouldn't be much deviation, so using slightly higher tolerances to allow for some deviation
	 * should suffice until you have packet loss, in which case de-syncing becomes quite normal
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Vault|AntiCheat")
	EVIAntiCheatType AntiCheatType;

public:
	/**
	 * Competitive anti-cheat. Does full vault checks and offers tolerances within which to compare the client's data. If tolerances are too tight may de-sync the client
	 * and reject the vault.
	 * Can get expensive with a lot of player characters, test cost with `stat VaultIt`
	 * Will always fail regardless if the ComputeVault result is false, consider using more lenient settings for the authority in VIPawnInterface::GetVaultTraceSettings()
	 * The results here are almost always identical due to prediction, low tolerances can be used, however collisions with other players may interfere
	 *
	 * Remember to allow for player collisions, packet loss, etc. that could prevent vaulting when not cheating
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Vault|AntiCheat")
	FVIAntiCheatSettings AntiCheatSettings;

public:
	UPROPERTY()
	bool bVaultAbilityInitialized;

	/** When true, player wants to vault */
	UPROPERTY(BlueprintReadOnly, Category = Vault)
	bool bPressedVault;

	UPROPERTY(BlueprintReadWrite, Category = Vault)
	FVICapsuleInfo CapsuleInfo;

protected:
	/** If true, last jump key resulted in vault press and releasing jump should release vault */
	bool bLastJumpInputVaulted;

	uint8 AutoVaultSkippedTicks;

	/** Was used in determining if jump key should vault or not; no need to do the check twice */
	FVIVaultResult PendingVaultResult;

	UPROPERTY(BlueprintReadOnly, Transient, DuplicateTransient, Category = Pawn)
	APawn* PawnOwner;

public:
	UPROPERTY()
	UAbilitySystemComponent* ASC;

public:
	UVIPawnVaultComponent()
		: AutoReleaseVaultInput(EVIVaultInputRelease::VIR_Always)
		, JumpKeyPriority(EVIJumpKeyPriority::JKP_SelectHighestPoint)
		, bVaultTraceComplex(false)
		, bCanVaultFromGround(true)
		, bCanVaultFromFalling(true)
		, bCanVaultFromSwimming(false)
		, bCanVaultFromCrouching(false)
		, AutoVaultStates(0)
		, AutoVaultCheckSkip(8)
		, AntiCheatType(EVIAntiCheatType::VIACT_None)
		, bVaultAbilityInitialized(false)
		, bPressedVault(false)
		, bLastJumpInputVaulted(false)
		, AutoVaultSkippedTicks(0)
		, PendingVaultResult(FVIVaultResult())
	{
		PrimaryComponentTick.bStartWithTickEnabled = false;
		PrimaryComponentTick.bCanEverTick = false;
	}

	virtual void BeginPlay() override;

	/**
	 * Call this from the Jump function override if using character
	 *
	 * You only need to supply the parameters that are used by your choice of JumpKeyPriority
	 * @param GravityZ: Used by "Select Highest Point"
	 * @param bCanJump: Used by "Only Vault from Air"
	 * @param bIsFalling: Used by "Only Vault from Air"
	 * @return True if jumping or false if vaulting
	 */
	UFUNCTION(BlueprintCallable, Category = Vault)
	virtual bool Jump(float GravityZ = 0.f, bool bCanJump = false, bool bIsFalling = false);

	/**
	 * Call this from StopJumping function override if using character
	 */
	UFUNCTION(BlueprintCallable, Category = Vault)
	virtual void StopJumping();

	/**
	 * Call this when vault input is pressed
	 */
	UFUNCTION(BlueprintCallable, Category = Vault)
	void Vault();

	/**
	 * Call this when vault input is released
	 */
	UFUNCTION(BlueprintCallable, Category = Vault)
	void StopVault();

	/**
	 * For characters this is called on CheckJumpInput() (see AVICharacterBase for example)
	 * For pawns it would be called on tick
	 * @param MovementMode: Used to identify AutoVault capability; if this isn't being used nothing needs to be passed
	 */
	UFUNCTION(BlueprintCallable, Category = Vault)
	virtual void CheckVaultInput(float DeltaTime, TEnumAsByte<enum EMovementMode> MovementMode = MOVE_Walking);

	/**
	 * @return True if vaulting
	 * Should be compatible with GAS as it uses gameplay tags for vaulting state
	 */
	UFUNCTION(BlueprintPure, Category = Vault)
	virtual bool IsVaulting() const;

	/**
	 * Compute the actual result of a vault attempt and return all information as well as whether it succeeded or not
	 * Always check CanVault() or similar functionality to ensure character or pawn is in a state where they're allowed to vault
	 * This performs multiple traces so use `stat VaultIt` to ensure you're not using too much CPU by over-using it
	 */
	UFUNCTION(BlueprintPure, Category = Vault)
	FVIVaultResult ComputeVault() const;

	/**
	 * Convert the data from ComputeVault into usable data
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = Vault)
	FVIVaultInfo ComputeVaultInfoFromResult(const FVIVaultInfo& VaultResult) const;

	/**
	 * @return True if the trace(s) allows us to vault
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Vault)
	bool ComputeShouldAutoVault();

protected:
	/**
	 * This is called on BeginPlay() to cache the default values from the owner
	 * If owner is a Character then it will simply use the capsule sizes
	 * If owner is a Pawn then you must subclass and override this
	 * 
	 * If overriding this in blueprint, you should first call IsCapsuleInfoValid() and return parent function if it's true and use custom logic if false
	 * If overriding this in C++, you should call CapsuleInfo.IsValidCapsule() and return super if it's true and use custom logic if false
	 * 
	 * If Pawn uses a box instead of a capsule, return the box HalfHeightt as the HalfHeight and the longest width as Radius
	 * 
	 * If it has a valid CapsuleInfo it will always return it
	 * 
	 * If capsule size changes eg. when crouching then you need to update the CapsuleInfo manually 
	 * (eg. from the OnStartCrouching and OnStopCrouching) events
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = Vault)
	FVICapsuleInfo GetCapsuleInfo() const;

	UFUNCTION(BlueprintPure, Category = Vault)
	bool IsCapsuleInfoValid() const { return CapsuleInfo.IsValidCapsule(); }

	// Anti-cheat
public:
	/**
	 * Tests for cheating or invalid / incompatible data
	 * Incompatible data and rejection can result from de-sync and does not necessarily mean the player is cheating
	 *
	 * No need to test local or remote role or net mode; this is all done for you
	 *
	 * @return True if anti-cheat passed
	 */
	UFUNCTION(BlueprintPure, Category = "Vault|AntiCheat")
	bool ComputeAntiCheatResult(const FVIVaultInfo& VaultInfo) const;

protected:
	/**
	 * Used by ComputeCustomAntiCheat to call built-in anti-cheat functions
	 * This allows extension of existing behaviour
	 * 
	 * ServerVaultResult can be obtained from ComputeVault()
	 *
	 * @return True if anti-cheat passed
	 */
	UFUNCTION(BlueprintPure, Category = "Vault|AntiCheat")
	bool ComputeDefaultAntiCheat(const FVIVaultInfo& ClientVaultInfo, const FVIVaultResult& ServerVaultResult) const { return AntiCheatSettings.ComputeAntiCheat(ClientVaultInfo, ServerVaultResult, PawnOwner); }

	/**
	 * Override to implement custom AntiCheat
	 * Note that performance metrics are not available for this function
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Vault|AntiCheat")
	bool ComputeCustomAntiCheat(const FVIVaultInfo& ClientVaultInfo) const;

protected:
	bool PredictLandingLocation(FPredictProjectilePathResult& PredictResult, float HalfHeight, float Radius, float GravityZ) const;
};
