// Copyright (c) 2019-2022 Drowning Dragons Limited. All Rights Reserved.

// Based on GASShooter by Dan Kestranek

#include "GAS/PlayMontageForMeshAndWait.h"
#include "GAS/VIAbilitySystemComponent.h"
#include "VITypes.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Character.h"

DEFINE_LOG_CATEGORY_STATIC(LogVIPlayMontage, Log, All);

DECLARE_CYCLE_STAT(TEXT("UPlayMontageForMeshAndWait Activate"), STAT_PLAYMONTAGEFORMESHANDWAIT_ACTIVATE, STATGROUP_VaultIt);

UPlayMontageForMeshAndWait::UPlayMontageForMeshAndWait(const FObjectInitializer& OI)
	: Super(OI)
{
	Rate = 1.f;
	bStopWhenAbilityEnds = true;
}

UPlayMontageForMeshAndWait* UPlayMontageForMeshAndWait::PlayMontageForMeshAndWait(UGameplayAbility* OwningAbility, FName TaskInstanceName, USkeletalMeshComponent* InMesh, UAnimMontage* MontageToPlay, float Rate /*= 1.f*/, FName StartSection /*= NAME_None*/, bool bStopWhenAbilityEnds /*= true*/, float AnimRootMotionTranslationScale /*= 1.f*/, bool bReplicateMontage /*= true*/, float OverrideBlendOutTimeForCancelAbility /*= -1.f*/, float OverrideBlendOutTimeForStopWhenEndAbility /*= -1.f*/)
{
	UAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Rate(Rate);

	UPlayMontageForMeshAndWait* MyObj = NewAbilityTask<UPlayMontageForMeshAndWait>(OwningAbility, TaskInstanceName);
	MyObj->Mesh = InMesh;
	MyObj->MontageToPlay = MontageToPlay;
	MyObj->Rate = Rate;
	MyObj->StartSection = StartSection;
	MyObj->AnimRootMotionTranslationScale = AnimRootMotionTranslationScale;
	MyObj->bStopWhenAbilityEnds = bStopWhenAbilityEnds;
	MyObj->bReplicateMontage = bReplicateMontage;
	MyObj->OverrideBlendOutTimeForCancelAbility = OverrideBlendOutTimeForCancelAbility;
	MyObj->OverrideBlendOutTimeForStopWhenEndAbility = OverrideBlendOutTimeForStopWhenEndAbility;

	return MyObj;
}

void UPlayMontageForMeshAndWait::Activate()
{
	if (!Ability)
	{
		return;
	}

	if (!Mesh)
	{
		UE_LOG(LogVIPlayMontage, Error, TEXT("UPlayMontageForMeshAndWait::Activate - invalid Mesh"));
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_PLAYMONTAGEFORMESHANDWAIT_ACTIVATE);

	bool bPlayedMontage = false;
	UVIAbilitySystemComponent* ASC = GetTargetASC();

	if (ASC)
	{
		const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
		UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
		if (AnimInstance != nullptr)
		{
			if (ASC->PlayMontageForMesh(Ability, Mesh, Ability->GetCurrentActivationInfo(), MontageToPlay, Rate, StartSection, bReplicateMontage) > 0.f)
			{
				// Playing a montage could potentially fire off a callback into game code which could kill this ability! Early out if we are  pending kill.
				if (ShouldBroadcastAbilityTaskDelegates() == false)
				{
					return;
				}

				CancelledHandle = Ability->OnGameplayAbilityCancelled.AddUObject(this, &UPlayMontageForMeshAndWait::OnAbilityCancelled);

				BlendingOutDelegate.BindUObject(this, &UPlayMontageForMeshAndWait::OnMontageBlendingOut);
				AnimInstance->Montage_SetBlendingOutDelegate(BlendingOutDelegate, MontageToPlay);

				MontageEndedDelegate.BindUObject(this, &UPlayMontageForMeshAndWait::OnMontageEnded);
				AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, MontageToPlay);

				ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
				if (Character && (Character->GetLocalRole() == ROLE_Authority ||
					(Character->GetLocalRole() == ROLE_AutonomousProxy && Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted)))
				{
					Character->SetAnimRootMotionTranslationScale(AnimRootMotionTranslationScale);
				}

				bPlayedMontage = true;
			}
		}
		else
		{
			UE_LOG(LogVIPlayMontage, Warning, TEXT("UPlayMontageForMeshAndWait call to PlayMontage failed!"));
		}
	}
	else
	{
		UE_LOG(LogVIPlayMontage, Warning, TEXT("UPlayMontageForMeshAndWait called on invalid AbilitySystemComponent"));
	}

	if (!bPlayedMontage)
	{
		UE_LOG(LogVIPlayMontage, Warning, TEXT("UPlayMontageForMeshAndWait called in Ability %s failed to play montage %s; Task Instance Name %s."), *Ability->GetName(), *GetNameSafe(MontageToPlay), *InstanceName.ToString());
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			//ABILITY_LOG(Display, TEXT("%s: OnCancelled"), *GetName());
			OnCancelled.Broadcast();
		}
	}

	SetWaitingOnAvatar();
}

void UPlayMontageForMeshAndWait::ExternalCancel()
{
	check(AbilitySystemComponent.IsValid());

	OnAbilityCancelled();

	Super::ExternalCancel();
}

void UPlayMontageForMeshAndWait::OnDestroy(bool AbilityEnded)
{
	// Note: Clearing montage end delegate isn't necessary since its not a multicast and will be cleared when the next montage plays.
	// (If we are destroyed, it will detect this and not do anything)
	if (Ability)
	{
		Ability->OnGameplayAbilityCancelled.Remove(CancelledHandle);
		if (AbilityEnded && bStopWhenAbilityEnds)
		{
			StopPlayingMontage(OverrideBlendOutTimeForStopWhenEndAbility);
		}
	}

	Super::OnDestroy(AbilityEnded);
}

bool UPlayMontageForMeshAndWait::StopPlayingMontage(float OverrideBlendOutTime /*= -1.f*/)
{
	if (!Mesh)
	{
		return false;
	}

	UVIAbilitySystemComponent* const ASC = GetTargetASC();
	if (!ASC)
	{
		return false;
	}

	const FGameplayAbilityActorInfo* const ActorInfo = Ability->GetCurrentActorInfo();
	if (!ActorInfo)
	{
		return false;
	}

	const UAnimInstance* const AnimInstance = Mesh->GetAnimInstance();
	if (!AnimInstance)
	{
		return false;
	}

	// Check if the montage is still playing
	// The ability would have been interrupted, in which case we should automatically stop the montage
	if (ASC && Ability)
	{
		if (ASC->GetAnimatingAbilityFromAnyMesh() == Ability
			&& ASC->GetCurrentMontageForMesh(Mesh) == MontageToPlay)
		{
			// Unbind delegates so they don't get called as well
			FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(MontageToPlay);
			if (MontageInstance)
			{
				MontageInstance->OnMontageBlendingOutStarted.Unbind();
				MontageInstance->OnMontageEnded.Unbind();
			}

			ASC->CurrentMontageStopForMesh(Mesh, OverrideBlendOutTime);
			return true;
		}
	}

	return false;
}

UVIAbilitySystemComponent* UPlayMontageForMeshAndWait::GetTargetASC() const
{
	return Cast<UVIAbilitySystemComponent>(AbilitySystemComponent);
}

void UPlayMontageForMeshAndWait::OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	if (Ability && Ability->GetCurrentMontage() == MontageToPlay)
	{
		if (Montage == MontageToPlay)
		{
			AbilitySystemComponent->ClearAnimatingAbility(Ability);

			// Reset AnimRootMotionTranslationScale
			ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
			if (Character && (Character->GetLocalRole() == ROLE_Authority ||
				(Character->GetLocalRole() == ROLE_AutonomousProxy && Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted)))
			{
				Character->SetAnimRootMotionTranslationScale(1.f);
			}

		}
	}

	if (bInterrupted)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnInterrupted.Broadcast();
		}
	}
	else
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnBlendOut.Broadcast();
		}
	}
}

void UPlayMontageForMeshAndWait::OnAbilityCancelled()
{
	// UE4 was calling the wrong callback

	if (StopPlayingMontage(OverrideBlendOutTimeForCancelAbility))
	{
		// Let the BP handle the interrupt as well
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCancelled.Broadcast();
		}
	}
}

void UPlayMontageForMeshAndWait::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!bInterrupted)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCompleted.Broadcast();
		}
	}

	EndTask();
}

FString UPlayMontageForMeshAndWait::GetDebugString() const
{
	const UAnimMontage* PlayingMontage = nullptr;
	if (Ability && Mesh)
	{
		const UAnimInstance* const AnimInstance = Mesh->GetAnimInstance();

		if (AnimInstance != nullptr)
		{
			PlayingMontage = AnimInstance->Montage_IsActive(MontageToPlay) ? MontageToPlay : AnimInstance->GetCurrentActiveMontage();
		}
	}

	return FString::Printf(TEXT("PlayMontageForMeshAndWait. MontageToPlay: %s  (Currently Playing): %s"), *GetNameSafe(MontageToPlay), *GetNameSafe(PlayingMontage));
}