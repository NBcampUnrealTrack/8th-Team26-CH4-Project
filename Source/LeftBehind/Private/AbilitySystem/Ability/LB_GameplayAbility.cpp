// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Ability/LB_GameplayAbility.h"

void ULB_GameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	if (bDrawDebug)
	{
		if (IsValid(GEngine))
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, FString::Printf(TEXT("%s Activated"),*GetName()));
		}
	}
}
