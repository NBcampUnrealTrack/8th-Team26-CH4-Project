// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Ability/Enemy/LB_HitReact.h"

void ULB_HitReact::CacheHitDirectionVectors(AActor* Instigator)
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!IsValid(Avatar) || !IsValid(Instigator))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LB HitReact] CacheHitDirectionVectors skipped. Avatar=%s Instigator=%s"),
			*GetNameSafe(Avatar), *GetNameSafe(Instigator));
		return;
	}

	AvatarForward = Avatar->GetActorForwardVector();

	const FVector AvatarLocation = Avatar->GetActorLocation();
	const FVector InstigatorLocation = Instigator->GetActorLocation();

	ToInstigator = InstigatorLocation - AvatarLocation;
	ToInstigator.Normalize();

}
