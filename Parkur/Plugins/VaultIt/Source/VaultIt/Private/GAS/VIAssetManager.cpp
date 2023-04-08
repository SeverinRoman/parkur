// Copyright (c) 2019-2022 Drowning Dragons Limited. All Rights Reserved.

#include "GAS/VIAssetManager.h"
#include "AbilitySystemGlobals.h"

void UVIAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();

	UAbilitySystemGlobals::Get().InitGlobalData();
}
