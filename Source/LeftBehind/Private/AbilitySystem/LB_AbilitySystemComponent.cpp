// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/LB_AbilitySystemComponent.h"
#include "GameplayTags/LBTags.h"
#include "AbilitySystem/LB_AbilitySystemComponent.h"
#include "AbilitySystem/LB_AttributeSet.h"


// Sets default values for this component's properties



void ULB_AbilitySystemComponent::OnGiveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	Super::OnGiveAbility(AbilitySpec);
	
	HandleAutoActivatedAbility(AbilitySpec);

}

void ULB_AbilitySystemComponent::OnRep_ActivateAbilities()
{
	Super::OnRep_ActivateAbilities();
}

void ULB_AbilitySystemComponent::HandleAutoActivatedAbility(const FGameplayAbilitySpec& AbilitySpec)
{
	if (!IsValid(AbilitySpec.Ability)) return;
	
	for (const FGameplayTag& Tag : AbilitySpec.Ability->GetAssetTags())
	{
		if (Tag.MatchesTagExact(LBTags::LBAbilities::ActivateOnGiven))
		{
			TryActivateAbility(AbilitySpec.Handle);
			return;
		}
	}
}
