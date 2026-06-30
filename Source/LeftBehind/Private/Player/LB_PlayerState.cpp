// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/LB_PlayerState.h"

#include "AbilitySystemComponent.h"
#include "NavigationSystemTypes.h"

ALB_PlayerState::ALB_PlayerState()
{
	SetNetUpdateFrequency(100.0f);
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
}

UAbilitySystemComponent* ALB_PlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}
