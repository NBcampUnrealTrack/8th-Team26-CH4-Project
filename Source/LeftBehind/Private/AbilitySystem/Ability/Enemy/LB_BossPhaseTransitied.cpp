// Fill out your copyright notice in the Description page of Project Settings.


#include "LB_BossPhaseTransitied.h"

void ULB_BossPhaseTransitied::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	
}

void ULB_BossPhaseTransitied::OnStunTagRemoved()
{
}

void ULB_BossPhaseTransitied::OnStunMontageFinished()
{
}
