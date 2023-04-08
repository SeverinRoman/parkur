// Copyright (c) 2019-2022 Drowning Dragons Limited. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "VIAssetManager.generated.h"

/**
 * 
 */
UCLASS()
class VAULTIT_API UVIAssetManager : public UAssetManager
{
	GENERATED_BODY()
	
public:
	virtual void StartInitialLoading() override;
};
