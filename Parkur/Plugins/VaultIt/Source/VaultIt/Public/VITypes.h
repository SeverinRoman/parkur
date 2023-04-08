// Copyright (c) 2019-2022 Drowning Dragons Limited. All Rights Reserved.

#pragma once

#include "Abilities/GameplayAbilityTargetTypes.h"
#include "GameplayTagsManager.h"
#include "VITypes.generated.h"

DECLARE_STATS_GROUP(TEXT("VaultIt_Plugin"), STATGROUP_VaultIt, STATCAT_Advanced);

/**
 * Used to determine behaviour of jump key in relation to vaulting
 */
UENUM(BlueprintType)
enum class EVIJumpKeyPriority : uint8
{
	JKP_DisableVault				UMETA(DisplayName = "Disable Vault from Jump", ToolTip = "Jump key cannot vault"),
	JKP_SelectHighestPoint			UMETA(DisplayName = "Select Highest Point", ToolTip = "Checks ahead to see if jump can reach as high as vault, and will vault if it can go higher"),
	JKP_AlwaysVault					UMETA(DisplayName = "Always Vault if Possible", ToolTip = "Will always vault if it is possible to do so, otherwise jump"),
	JKP_OnlyVaultFromAir			UMETA(DisplayName = "Only Vault from Air", ToolTip = "Only vault if in air and jump key wont otherwise do anything (eg. double jump)"),
	JKP_MAX = 255					UMETA(Hidden)
};

/** How gameplay effects will be replicated to clients */
UENUM(BlueprintType)
enum class EVIGameplayEffectReplicationMode : uint8
{
	/** Only replicate minimal gameplay effect info. Note: this does not work for Owned AbilitySystemComponents (Use Mixed instead). */
	Minimal,
	/** Only replicate minimal gameplay effect info to simulated proxies but full info to owners and autonomous proxies */
	Mixed,
	/** Replicate full gameplay info to all */
	Full,
};

USTRUCT(BlueprintType)
struct VAULTIT_API FVICapsuleInfo
{
	GENERATED_BODY()

	FVICapsuleInfo()
		: HalfHeight(0.f)
		, Radius(0.f)
	{}

	FVICapsuleInfo(float InHalfHeight, float InRadius)
		: HalfHeight(InHalfHeight)
		, Radius(InRadius)
	{}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vault)
	float HalfHeight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vault)
	float Radius;

	float GetHalfHeightWithoutHemisphere() const { return HalfHeight - Radius; }

	bool IsValidCapsule() const { return HalfHeight > 0.f && Radius > 0.f; }
};

/**
 * Used to create a nested array of animations
 * TMap<float, FVIAnimSet> where float is vault height
 */
USTRUCT(BlueprintType)
struct VAULTIT_API FVIAnimations
{
	GENERATED_BODY()

	FVIAnimations()
	{}

	UPROPERTY(EditDefaultsOnly, BlueprintReadonly, Category = Vault)
	TArray<UAnimMontage*> Animations;

	bool IsValid() const { return Animations.Num() > 0; }
};

/**
 * Used to create a nested array of animations
 * TMap<float, FVIAnimSet> where float is vault height
 */
USTRUCT(BlueprintType)
struct VAULTIT_API FVIAnimSet
{
	GENERATED_BODY()

	FVIAnimSet()
	{}

	/**
	 * Key: Vault height
	 * Value: Array of animations that can be played at that height
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadonly, Category = Vault)
	TMap<float, FVIAnimations> Animations;

	bool IsValid() const { return Animations.Num() > 0; }
};

/**
 * Settings to use when tracing for a potential vault attempt
 */
USTRUCT(BlueprintType)
struct VAULTIT_API FVITraceSettings
{
	GENERATED_BODY()

	FVITraceSettings()
		: MaxLedgeHeight(250.f)
		, MinLedgeHeight(45.f)
		, ReachDistance(75.f)
		, ForwardTraceRadius(30.f)
		, DownwardTraceRadius(30.f)
		, CollisionFloatHeight(2.4f)
		, MaxObjectVelocity(0.f)
		, ObjectChannels({ ECollisionChannel::ECC_WorldStatic, ECollisionChannel::ECC_WorldDynamic })
		, TraceProfile(TEXT("Vault"))
	{}

	/** This should be near the highest vault animation you have - motion warping can assist a lot but not completely */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Vault|Settings")
	float MaxLedgeHeight;

	/** This should be the lowest vault animation you have and should probably not be identical to or lower than CMC max step height */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Vault|Settings")
	float MinLedgeHeight;

	/** How far ahead of the character can we grab when looking for a ledge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Vault|Settings")
	float ReachDistance;

	/** Radius when checking if the surface ahead is walkable (no need to vault it) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Vault|Settings")
	float ForwardTraceRadius;

	/** Radius when checking if we can stand on the surface ahead (and below) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Vault|Settings")
	float DownwardTraceRadius;
	
	/** Characters float 2.4cm off the ground by default */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Vault|Settings")
	float CollisionFloatHeight;

	/** Object can not be vaulted onto if exceeding this velocity. Set to 0 to disable. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Vault|Settings")
	float MaxObjectVelocity;

	/** Channels for object types that we can vault on (Must be an object channel - eg. WorldStatic; trace channels will not work - eg. Visibility, Camera) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Vault|Settings")
	TArray<TEnumAsByte<ECollisionChannel>> ObjectChannels;

	/** This should be a profile that tests for anything that would stop us moving to our vault location */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Vault|Settings")
	FName TraceProfile;

	/** Helper for conversion of ObjectChannels */
	TArray<TEnumAsByte<EObjectTypeQuery>> GetObjectTypes() const
	{
		TArray<TEnumAsByte<EObjectTypeQuery>> Result;
		for (const ECollisionChannel& CC : ObjectChannels)
		{
			Result.Add(UEngineTypes::ConvertToObjectType(CC));
		}

		return Result;
	}
};

USTRUCT(BlueprintType)
struct VAULTIT_API FVIBoneFBIKData
{
	GENERATED_BODY()

public:
	FVIBoneFBIKData()
		: BoneName(NAME_None)
		, InterpRate(150.f)
		, bEnabled(false)
		, Location(FVector::ZeroVector)
		, TargetLocation(FVector::ZeroVector)
		, bReset(true)
	{}

	FVIBoneFBIKData(const FName& InBoneName, float InInterpRate = 150.f)
		: BoneName(InBoneName)
		, InterpRate(InInterpRate)
		, bEnabled(false)
		, Location(FVector::ZeroVector)
		, TargetLocation(FVector::ZeroVector)
		, bReset(true)
	{}

	/** Bone that is moved by FBIK */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vault)
	FName BoneName;

	/** Rate that bone moves to target location. 0 for no interpolation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vault)
	float InterpRate;

	/** Whether this bone has it's FBIK enabled or not */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Vault)
	bool bEnabled;

	/** Location used by anim graph */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Vault)
	FVector Location;

	/** Location to interpolate to */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Vault)
	FVector TargetLocation;

protected:
	UPROPERTY()
	bool bReset;

public:
	void Update(float DeltaTime)
	{
		if (!bEnabled)
		{
			return;
		}

		if (bReset || InterpRate <= 0.f)
		{
			Location = TargetLocation;
			bReset = false;
		}
		else
		{
			Location = FMath::VInterpConstantTo(Location, TargetLocation, DeltaTime, InterpRate);
		}
	}

	/** Enable or update target location */
	void Enable(const FVector& NewLocation)
	{
		bEnabled = true;
		TargetLocation = NewLocation;

		// Init Location
		if (Location.IsNearlyZero())
		{
			Location = TargetLocation;
		}
	}

	void Disable()
	{
		// Leave Location intact for blending out
		bEnabled = false;
		bReset = true;
		TargetLocation = FVector::ZeroVector;
	}
};

/**
 * Used by simulated proxies to motion match
 * Net quantized to 1 decimal point for bandwidth optimization
 */
USTRUCT(BlueprintType)
struct VAULTIT_API FVIRepMotionMatch
{
	GENERATED_BODY()

	FVIRepMotionMatch()
		: Location(FVector::ZeroVector)
		, Direction(FVector::ZeroVector)
	{}

	FVIRepMotionMatch(const FVector& InLocation, const FVector& InDirection)
		: Location(InLocation)
		, Direction(InDirection)
	{}

	UPROPERTY(BlueprintReadWrite, Category = Vault)
	FVector Location;

	UPROPERTY(BlueprintReadWrite, Category = Vault)
	FVector Direction;

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FVIRepMotionMatch> : public TStructOpsTypeTraitsBase2<FVIRepMotionMatch>
{
	enum
	{
		WithNetSerializer = true
	};
};

/**
 * Vault info computed locally then sent to the server for use by GA_Vault
 * Net quantized to 1 decimal point for bandwidth optimization
 */
USTRUCT(BlueprintType)
struct VAULTIT_API FVIVaultInfo
{
	GENERATED_BODY()

	FVIVaultInfo()
		: Location(FVector::ZeroVector)
		, Direction(FVector::ZeroVector)
		, Height(0.f)
		, RandomSeed(0)
	{}

	UPROPERTY(BlueprintReadOnly, Category = Vault)
	FVector Location;

	UPROPERTY(BlueprintReadOnly, Category = Vault)
	FVector Direction;

	UPROPERTY(BlueprintReadOnly, Category = Vault)
	float Height;

	UPROPERTY(BlueprintReadOnly, Category = Vault)
	uint8 RandomSeed;

	void SetHeight(float InHeight)
	{
		// 1 decimal point of precision
		Height = RoundFloat(InHeight);
	}

	float RoundFloat(float InFloat) const
	{
		// 1 decimal point of precision
		return FMath::RoundToFloat(InFloat * 10.f) / 10.f;
	}

	bool IsValid() const { return Height != 0.f; }
};

/**
 * Result of the vault along with the VaultInfo
 */
USTRUCT(BlueprintType)
struct VAULTIT_API FVIVaultResult : public FVIVaultInfo
{
	GENERATED_BODY()

	FVIVaultResult()
		: bSuccess(false)
	{}

	UPROPERTY(BlueprintReadOnly, Category = Vault)
	bool bSuccess;
};

/**
 * GameplayAbilityTargetData responsible for sending our VaultInfo to the GameplayAbility
 */
USTRUCT(BlueprintType)
struct VAULTIT_API FVIGameplayAbilityTargetData_VaultInfo : public FGameplayAbilityTargetData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FVIVaultInfo VaultInfo;

	const FVIVaultInfo* GetVaultInfo() const
	{
		return &VaultInfo;
	}

	// -------------------------------------

	virtual bool HasOrigin() const override final { return false; }
	virtual FTransform GetOrigin() const override final { return FTransform::Identity; }

	virtual bool HasEndPoint() const override final { return false; }
	virtual FVector GetEndPoint() const override final { return FVector::ZeroVector; }

	// -------------------------------------

	virtual UScriptStruct* GetScriptStruct() const override final
	{
		return FVIGameplayAbilityTargetData_VaultInfo::StaticStruct();
	}

	virtual FString ToString() const override final
	{
		return TEXT("FVIGameplayAbilityTargetData_VaultInfo");
	}

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FVIGameplayAbilityTargetData_VaultInfo> : public TStructOpsTypeTraitsBase2<FVIGameplayAbilityTargetData_VaultInfo>
{
	enum
	{
		WithNetSerializer = true	// For now this is REQUIRED for FGameplayAbilityTargetDataHandle net serialization to work
	};
};