// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/LB_EnemyCharacter.h"

#include "AbilitySystem/LB_AbilitySystemComponent.h"
#include "AbilitySystem/LB_AttributeSet.h"

ALB_EnemyCharacter::ALB_EnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	
	AbilitySystemComponent = CreateDefaultSubobject<ULB_AbilitySystemComponent>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	
	Attributeset = CreateDefaultSubobject<ULB_AttributeSet>("AttributeSet");
	
}

void ALB_EnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	if (!IsValid(GetAbilitySystemComponent()))
	{
		return;
	}
	
	GetAbilitySystemComponent()->InitAbilityActorInfo(this,this);
	OnAscInitialized.Broadcast(GetAbilitySystemComponent(), GetAttributeSet());
	if (!HasAuthority())
	{
		return;
	}
	
	GiveStartupAbilities();
	InitializeAttribute();
	
}

UAttributeSet* ALB_EnemyCharacter::GetAttributeSet() const
{
	return Attributeset;
}

UAbilitySystemComponent* ALB_EnemyCharacter:: GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}


