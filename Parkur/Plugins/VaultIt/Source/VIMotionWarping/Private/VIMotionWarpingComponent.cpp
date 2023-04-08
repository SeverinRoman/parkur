// Copyright Epic Games, Inc. All Rights Reserved.

#include "VIMotionWarpingComponent.h"

#include "VIRootMotionModifier.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimationPoseData.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "VIAnimNotifyState_MotionWarping.h"

DEFINE_LOG_CATEGORY(LogVIMotionWarping);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
TAutoConsoleVariable<int32> FVIMotionWarpingCVars::CVarVIMotionWarpingDisable(TEXT("a.VIMotionWarping.Disable"), 0, TEXT("Disable Motion Warping"), ECVF_Cheat);
TAutoConsoleVariable<int32> FVIMotionWarpingCVars::CVarVIMotionWarpingDebug(TEXT("a.VIMotionWarping.Debug"), 0, TEXT("0: Disable, 1: Only Log 2: Log and DrawDebug"), ECVF_Cheat);
TAutoConsoleVariable<float> FVIMotionWarpingCVars::CVarVIMotionWarpingDrawDebugDuration(TEXT("a.VIMotionWarping.DrawDebugLifeTime"), 0.f, TEXT("Time in seconds each draw debug persists.\nRequires 'a.VIMotionWarping.Debug 2'"), ECVF_Cheat);
#endif

// UVIMotionWarpingUtilities
///////////////////////////////////////////////////////////////////////

void UVIMotionWarpingUtilities::BreakVIMotionWarpingSyncPoint(const FVIMotionWarpingSyncPoint& SyncPoint, FVector& Location, FRotator& Rotation)
{
	Location = SyncPoint.GetLocation(); 
	Rotation = SyncPoint.Rotator();
}

FVIMotionWarpingSyncPoint UVIMotionWarpingUtilities::MakeVIMotionWarpingSyncPoint(FVector Location, FRotator Rotation)
{
	return FVIMotionWarpingSyncPoint(Location, Rotation);
}

void UVIMotionWarpingUtilities::ExtractLocalSpacePose(const UAnimSequenceBase* Animation, const FBoneContainer& BoneContainer, float Time, bool bExtractRootMotion, FCompactPose& OutPose)
{
	OutPose.SetBoneContainer(&BoneContainer);

	FBlendedCurve Curve;
	Curve.InitFrom(BoneContainer);

	const FAnimExtractContext Context(Time, bExtractRootMotion);

	UE::Anim::FStackAttributeContainer Attributes;
	FAnimationPoseData AnimationPoseData(OutPose, Curve, Attributes);
	if (const UAnimSequence* AnimSequence = Cast<UAnimSequence>(Animation))
	{
		AnimSequence->GetBonePose(AnimationPoseData, Context);
	}
	else if (const UAnimMontage* AnimMontage = Cast<UAnimMontage>(Animation))
	{
		const FAnimTrack& AnimTrack = AnimMontage->SlotAnimTracks[0].AnimTrack;
		AnimTrack.GetAnimationPose(AnimationPoseData, Context);
	}
}

void UVIMotionWarpingUtilities::ExtractComponentSpacePose(const UAnimSequenceBase* Animation, const FBoneContainer& BoneContainer, float Time, bool bExtractRootMotion, FCSPose<FCompactPose>& OutPose)
{
	FCompactPose Pose;
	ExtractLocalSpacePose(Animation, BoneContainer, Time, bExtractRootMotion, Pose);
	OutPose.InitPose(MoveTemp(Pose));
}

FTransform UVIMotionWarpingUtilities::ExtractVIRootMotionFromAnimation(const UAnimSequenceBase* Animation, float StartTime, float EndTime)
{
	if (const UAnimMontage* Anim = Cast<UAnimMontage>(Animation))
	{
		return Anim->ExtractRootMotionFromTrackRange(StartTime, EndTime);
	}

	if (const UAnimSequence* Anim = Cast<UAnimSequence>(Animation))
	{
		return Anim->ExtractRootMotionFromRange(StartTime, EndTime);
	}

	return FTransform::Identity;
}

void UVIMotionWarpingUtilities::GetVIMotionWarpingWindowsFromAnimation(const UAnimSequenceBase* Animation, TArray<FVIMotionWarpingWindowData>& OutWindows)
{
	if(Animation)
	{
		OutWindows.Reset();

		for (int32 Idx = 0; Idx < Animation->Notifies.Num(); Idx++)
		{
			const FAnimNotifyEvent& NotifyEvent = Animation->Notifies[Idx];
			if (UVIAnimNotifyState_MotionWarping* Notify = Cast<UVIAnimNotifyState_MotionWarping>(NotifyEvent.NotifyStateClass))
			{
				FVIMotionWarpingWindowData Data;
				Data.VIAnimNotify = Notify;
				Data.StartTime = NotifyEvent.GetTriggerTime();
				Data.EndTime = NotifyEvent.GetEndTriggerTime();
				OutWindows.Add(Data);
			}
		}
	}
}

void UVIMotionWarpingUtilities::GetVIMotionWarpingWindowsForSyncPointFromAnimation(const UAnimSequenceBase* Animation, FName SyncPointName, TArray<FVIMotionWarpingWindowData>& OutWindows)
{
	if (Animation && SyncPointName != NAME_None)
	{
		OutWindows.Reset();

		for (int32 Idx = 0; Idx < Animation->Notifies.Num(); Idx++)
		{
			const FAnimNotifyEvent& NotifyEvent = Animation->Notifies[Idx];
			if (UVIAnimNotifyState_MotionWarping* Notify = Cast<UVIAnimNotifyState_MotionWarping>(NotifyEvent.NotifyStateClass))
			{
				if (const UVIRootMotionModifierConfig_Warp* Config = Cast<const UVIRootMotionModifierConfig_Warp>(Notify->VIRootMotionModifierConfig))
				{
					if(Config->SyncPointName == SyncPointName)
					{
						FVIMotionWarpingWindowData Data;
						Data.VIAnimNotify = Notify;
						Data.StartTime = NotifyEvent.GetTriggerTime();
						Data.EndTime = NotifyEvent.GetEndTriggerTime();
						OutWindows.Add(Data);
					}
				}
			}
		}
	}
}

// UVIMotionWarpingComponent
///////////////////////////////////////////////////////////////////////

UVIMotionWarpingComponent::UVIMotionWarpingComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsInitializeComponent = true;
}

void UVIMotionWarpingComponent::InitializeComponent()
{
	Super::InitializeComponent();

	CharacterOwner = Cast<ACharacter>(GetOwner());

	UCharacterMovementComponent* CharacterMovementComp = CharacterOwner.IsValid() ? CharacterOwner->GetCharacterMovement() : nullptr;
	if (CharacterMovementComp)
	{
		CharacterMovementComp->ProcessRootMotionPreConvertToWorld.BindUObject(this, &UVIMotionWarpingComponent::ProcessRootMotionPreConvertToWorld);
		CharacterMovementComp->ProcessRootMotionPostConvertToWorld.BindUObject(this, &UVIMotionWarpingComponent::ProcessRootMotionPostConvertToWorld);
	}
}

bool UVIMotionWarpingComponent::ContainsModifier(const UAnimSequenceBase* Animation, float StartTime, float EndTime) const
{
	return VIRootMotionModifiers.ContainsByPredicate([=](const TSharedPtr<FVIRootMotionModifier>& Modifier)
		{
			return (Modifier.IsValid() && Modifier->Animation == Animation && Modifier->StartTime == StartTime && Modifier->EndTime == EndTime);
		});
}

void UVIMotionWarpingComponent::AddVIRootMotionModifier(TSharedPtr<FVIRootMotionModifier> Modifier)
{
	if (ensureAlways(Modifier.IsValid()))
	{
		VIRootMotionModifiers.Add(Modifier);

		UE_LOG(LogVIMotionWarping, Verbose, TEXT("VIMotionWarping: VIRootMotionModifier added. NetMode: %d WorldTime: %f Char: %s Animation: %s [%f %f] [%f %f] Loc: %s Rot: %s"),
			GetWorld()->GetNetMode(), GetWorld()->GetTimeSeconds(), *GetNameSafe(GetCharacterOwner()), *GetNameSafe(Modifier->Animation.Get()), Modifier->StartTime, Modifier->EndTime, Modifier->PreviousPosition, Modifier->CurrentPosition,
			*GetCharacterOwner()->GetActorLocation().ToString(), *GetCharacterOwner()->GetActorRotation().ToCompactString());
	}
}

void UVIMotionWarpingComponent::DisableAllVIRootMotionModifiers()
{
	if (VIRootMotionModifiers.Num() > 0)
	{
		for (TSharedPtr<FVIRootMotionModifier>& Modifier : VIRootMotionModifiers)
		{
			Modifier->State = EVIRootMotionModifierState::Disabled;
		}
	}
}

void UVIMotionWarpingComponent::Update()
{
	check(GetCharacterOwner());

	const FAnimMontageInstance* VIRootMotionMontageInstance = GetCharacterOwner()->GetRootMotionAnimMontageInstance();
	UAnimMontage* Montage = VIRootMotionMontageInstance ? VIRootMotionMontageInstance->Montage : nullptr;
	if (Montage)
	{
		const float PreviousPosition = VIRootMotionMontageInstance->GetPreviousPosition();
		// const float CurrentPosition = VIRootMotionMontageInstance->GetPosition();

		// Loop over notifies directly in the montage, looking for Motion Warping windows
		for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
		{
			const UVIAnimNotifyState_MotionWarping* const VIMotionWarpingNotify = NotifyEvent.NotifyStateClass ? Cast<UVIAnimNotifyState_MotionWarping>(NotifyEvent.NotifyStateClass) : nullptr;
			if (VIMotionWarpingNotify)
			{
				const float StartTime = FMath::Clamp(NotifyEvent.GetTriggerTime(), 0.f, Montage->GetPlayLength());
				const float EndTime = FMath::Clamp(NotifyEvent.GetEndTriggerTime(), 0.f, Montage->GetPlayLength());

				if (PreviousPosition >= StartTime && PreviousPosition < EndTime)
				{
					if (!ContainsModifier(Montage, StartTime, EndTime))
					{
						VIMotionWarpingNotify->AddVIRootMotionModifier(this, Montage, StartTime, EndTime);
					}
				}
			}
		}

		if(bSearchForWindowsInAnimsWithinMontages)
		{
			// Same as before but scanning all animation within the montage
			for (int32 SlotIdx = 0; SlotIdx < Montage->SlotAnimTracks.Num(); SlotIdx++)
			{
				const FAnimTrack& AnimTrack = Montage->SlotAnimTracks[SlotIdx].AnimTrack;
				const FAnimSegment* AnimSegment = AnimTrack.GetSegmentAtTime(PreviousPosition);
				if (AnimSegment && AnimSegment->GetAnimReference())
				{
					for (const FAnimNotifyEvent& NotifyEvent : AnimSegment->GetAnimReference()->Notifies)
					{
						const UVIAnimNotifyState_MotionWarping* VIMotionWarpingNotify = NotifyEvent.NotifyStateClass ? Cast<UVIAnimNotifyState_MotionWarping>(NotifyEvent.NotifyStateClass) : nullptr;
						if (VIMotionWarpingNotify)
						{
							const float NotifyStartTime = FMath::Clamp(NotifyEvent.GetTriggerTime(), 0.f, AnimSegment->GetAnimReference()->GetPlayLength());
							const float NotifyEndTime = FMath::Clamp(NotifyEvent.GetEndTriggerTime(), 0.f, AnimSegment->GetAnimReference()->GetPlayLength());

							// Convert notify times from AnimSequence times to montage times
							const float StartTime = (NotifyStartTime - AnimSegment->AnimStartTime) + AnimSegment->StartPos;
							const float EndTime = (NotifyEndTime - AnimSegment->AnimStartTime) + AnimSegment->StartPos;

							if (PreviousPosition >= StartTime && PreviousPosition < EndTime)
							{
								if (!ContainsModifier(Montage, StartTime, EndTime))
								{
									VIMotionWarpingNotify->AddVIRootMotionModifier(this, Montage, StartTime, EndTime);
								}
							}
						}
					}
				}
			}
		}
	}

	OnPreUpdate.Broadcast(this);

	// Update the state of all the modifiers
	if(VIRootMotionModifiers.Num() > 0)
	{
		for (const TSharedPtr<FVIRootMotionModifier>& Modifier : VIRootMotionModifiers)
		{
			Modifier->Update(*this);
		}

		// Remove the modifiers that has been marked for removal
		VIRootMotionModifiers.RemoveAll([this](const TSharedPtr<FVIRootMotionModifier>& Modifier) 
		{ 
			if(Modifier->State == EVIRootMotionModifierState::MarkedForRemoval)
			{
				UE_LOG(LogVIMotionWarping, Verbose, TEXT("VIMotionWarping: VIRootMotionModifier removed. NetMode: %d WorldTime: %f Char: %s Animation: %s [%f %f] [%f %f] Loc: %s Rot: %s"),
					GetWorld()->GetNetMode(), GetWorld()->GetTimeSeconds(), *GetNameSafe(GetCharacterOwner()), *GetNameSafe(Modifier->Animation.Get()), Modifier->StartTime, Modifier->EndTime, Modifier->PreviousPosition, Modifier->CurrentPosition,
					*GetCharacterOwner()->GetActorLocation().ToString(), *GetCharacterOwner()->GetActorRotation().ToCompactString());

				return true;
			}

			return false; 
		});
	}
}

FTransform UVIMotionWarpingComponent::ProcessRootMotionPreConvertToWorld(const FTransform& InVIRootMotion, UCharacterMovementComponent* CharacterMovementComponent, float DeltaSeconds)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (FVIMotionWarpingCVars::CVarVIMotionWarpingDisable.GetValueOnGameThread() > 0)
	{
		return InVIRootMotion;
	}
#endif

	// Check for warping windows and update modifier states
	Update();

	FTransform FinalVIRootMotion = InVIRootMotion;

	// Apply Local Space Modifiers
	for (const TSharedPtr<FVIRootMotionModifier>& VIRootMotionModifier : VIRootMotionModifiers)
	{
		if (VIRootMotionModifier->State == EVIRootMotionModifierState::Active && VIRootMotionModifier->bInLocalSpace)
		{
			FinalVIRootMotion = VIRootMotionModifier->ProcessVIRootMotion(*this, FinalVIRootMotion, DeltaSeconds);
		}
	}

	return FinalVIRootMotion;
}

FTransform UVIMotionWarpingComponent::ProcessRootMotionPostConvertToWorld(const FTransform& InVIRootMotion, UCharacterMovementComponent* CharacterMovementComponent, float DeltaSeconds)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (FVIMotionWarpingCVars::CVarVIMotionWarpingDisable.GetValueOnGameThread() > 0)
	{
		return InVIRootMotion;
	}
#endif

	FTransform FinalVIRootMotion = InVIRootMotion;

	// Apply World Space Modifiers
	for (const TSharedPtr<FVIRootMotionModifier>& VIRootMotionModifier : VIRootMotionModifiers)
	{
		if (VIRootMotionModifier->State == EVIRootMotionModifierState::Active && !VIRootMotionModifier->bInLocalSpace)
		{
			FinalVIRootMotion = VIRootMotionModifier->ProcessVIRootMotion(*this, FinalVIRootMotion, DeltaSeconds);
		}
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const int32 DebugLevel = FVIMotionWarpingCVars::CVarVIMotionWarpingDebug.GetValueOnGameThread();
	if (DebugLevel >= 2)
	{
		const float DrawDebugDuration = FVIMotionWarpingCVars::CVarVIMotionWarpingDrawDebugDuration.GetValueOnGameThread();
		constexpr float PointSize = 7.f;
		const FVector ActorFeetLocation = CharacterMovementComponent->GetActorFeetLocation();
		if (VIRootMotionModifiers.Num() > 0)
		{
			if (!OriginalVIRootMotionAccum.IsSet())
			{
				OriginalVIRootMotionAccum = ActorFeetLocation;
				WarpedVIRootMotionAccum = ActorFeetLocation;
			}
			
			OriginalVIRootMotionAccum = OriginalVIRootMotionAccum.GetValue() + InVIRootMotion.GetLocation();
			WarpedVIRootMotionAccum = WarpedVIRootMotionAccum.GetValue() + FinalVIRootMotion.GetLocation();

			DrawDebugPoint(GetWorld(), OriginalVIRootMotionAccum.GetValue(), PointSize, FColor::Red, false, DrawDebugDuration, 0);
			DrawDebugPoint(GetWorld(), WarpedVIRootMotionAccum.GetValue(), PointSize, FColor::Green, false, DrawDebugDuration, 0);
		}
		else
		{
			OriginalVIRootMotionAccum.Reset();
			WarpedVIRootMotionAccum.Reset();
		}

		DrawDebugPoint(GetWorld(), ActorFeetLocation, PointSize, FColor::Blue, false, DrawDebugDuration, 0);
	}
#endif

	return FinalVIRootMotion;
}

void UVIMotionWarpingComponent::AddOrUpdateSyncPoint(FName Name, const FVIMotionWarpingSyncPoint& SyncPoint)
{
	if (Name != NAME_None)
	{
		FVIMotionWarpingSyncPoint& VIMotionWarpingSyncPoint = SyncPoints.FindOrAdd(Name);
		VIMotionWarpingSyncPoint = SyncPoint;
	}
}

int32 UVIMotionWarpingComponent::RemoveSyncPoint(FName Name)
{
	return SyncPoints.Remove(Name);
}