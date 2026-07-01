// Fill out your copyright notice in the Description page of Project Settings.


#include "LeftBehind/Public/Characters/LB_BaseCharacter.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"


// Sets default values
ALB_BaseCharacter::ALB_BaseCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}

UAbilitySystemComponent* ALB_BaseCharacter::GetAbilitySystemComponent() const
{
	return nullptr;
}

UAttributeSet* ALB_BaseCharacter::GetAttributeSet() const
{
	return nullptr;
}

void ALB_BaseCharacter::GiveStartupAbilities()
{
	if (!IsValid(GetAbilitySystemComponent())) return;
	for (const auto& Abilities : StartupAbilities)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(Abilities);
		GetAbilitySystemComponent()->GiveAbility(AbilitySpec);
	}
	
}

void ALB_BaseCharacter::InitializeAttribute() const
{
	checkf(IsValid(InitializeAttributesEffect), TEXT("Initialize Attributes Effect not set"));
	
	FGameplayEffectContextHandle ContextHandle = GetAbilitySystemComponent()->MakeEffectContext();
	FGameplayEffectSpecHandle SpecHandle =  GetAbilitySystemComponent()->MakeOutgoingSpec(InitializeAttributesEffect,1.f,ContextHandle);
	GetAbilitySystemComponent()->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	
}



