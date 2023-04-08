// Copyright (c) 2019-2022 Drowning Dragons Limited. All Rights Reserved.

#include "Animation/VIAnimInstanceFP.h"
#include "Pawn/VICharacterBase.h"
#include "VIBlueprintFunctionLibrary.h"

bool UVIAnimInstanceFP::CanPlayFPMontage() const
{
	return Character && Character->IsLocallyControlled() && !Character->IsBotControlled();
}

void UVIAnimInstanceFP::OnStartVault()
{
	Super::OnStartVault();

	if (CanPlayFPMontage())
	{
		if (MontageLinkMap.Contains(Character->GetCurrentMontage()))
		{
			// Find the FP animation that corresponds to the current TP animation
			MontageToPlay = MontageLinkMap.FindChecked(Character->GetCurrentMontage());
		}

		if (MontageToPlay)
		{
			// Play the animation, borrowing the length and blend out time from the TP animation
			VaultBlendOutTime = Character->GetCurrentMontage()->GetDefaultBlendOutTime();
			const float PlayLength = Character->GetCurrentMontage()->GetPlayLength();
			const float PlayRate = UVIBlueprintFunctionLibrary::ComputeAnimationPlayRateFromDuration(MontageToPlay, PlayLength);
			Montage_Play(MontageToPlay, PlayRate);
		}
		else
		{
			// Should have been able to play an animation but it doesn't exist
			UVIBlueprintFunctionLibrary::MessageLogError(FString::Printf(TEXT("{ %s} No FP Vault Animation found for { %s }"), *Character->GetName(), *Character->GetCurrentMontage()->GetName()));
		}
	}
}

void UVIAnimInstanceFP::OnStopVault()
{
	Super::OnStopVault();

	// Stop the montage if vaulting ended
	if (MontageToPlay)
	{
		Montage_Stop(VaultBlendOutTime, MontageToPlay);
		MontageToPlay = nullptr;
	}
}
