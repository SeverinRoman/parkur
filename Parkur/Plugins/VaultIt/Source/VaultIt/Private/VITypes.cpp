// Copyright (c) 2019-2022 Drowning Dragons Limited. All Rights Reserved.

#include "VITypes.h"

bool FVIGameplayAbilityTargetData_VaultInfo::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	FVector_NetQuantize10 VaultLocation = FVector_NetQuantize10(VaultInfo.Location);
	FVector_NetQuantize10 VaultDirection = FVector_NetQuantize10(VaultInfo.Direction);
	VaultLocation.NetSerialize(Ar, Map, bOutSuccess);
	VaultDirection.NetSerialize(Ar, Map, bOutSuccess);
	
	Ar << VaultInfo.Height;
	Ar << VaultInfo.RandomSeed;
	
	if (Ar.IsLoading())
	{
		VaultInfo.Location = VaultLocation;
		VaultInfo.Direction = VaultDirection;
	}
	
	bOutSuccess = true;
	return true;
}

bool FVIRepMotionMatch::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	FVector_NetQuantize10 NetLocation = FVector_NetQuantize10(Location);
	FVector_NetQuantize10 NetDirection = FVector_NetQuantize10(Direction);
	NetLocation.NetSerialize(Ar, Map, bOutSuccess);
	NetDirection.NetSerialize(Ar, Map, bOutSuccess);
	
	if (Ar.IsLoading())
	{
		Location = NetLocation;
		Direction = NetDirection;
	}
	
	bOutSuccess = true;
	return true;
}
