// Fill out your copyright notice in the Description page of Project Settings.


#include "Controller/Component/LB_AttackPatternComponent.h"
#include "AbilitySystem/LB_AbilitySystemComponent.h"
#include "Characters/Boss/LB_BossCharacter.h"
#include "Controller/LB_BossController.h"


// Sets default values for this component's properties
ULB_AttackPatternComponent::ULB_AttackPatternComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

}

void ULB_AttackPatternComponent::SelectAttackPattern()
{
	ALB_BossController* BossController=  Cast<ALB_BossController>(GetOwner());
	if (!BossController) return;
	
	ALB_BossCharacter* BossCharacter = Cast<ALB_BossCharacter>(BossController->GetPawn());
	if (!BossCharacter) return;
	
	CacahedASC = Cast<ULB_AbilitySystemComponent>(BossCharacter->GetAbilitySystemComponent());
	if (!CacahedASC) return;
	
	TArray<FGameplayAbilitySpec*> SkillList;
	FGameplayTag SearchTag = FGameplayTag::RequestGameplayTag(TEXT("LBTags.Abilities"));
	
	for (FGameplayAbilitySpec& Spec : CacahedASC->GetActivatableAbilities())
	{
		if (!Spec.Ability->AbilityTags.HasTag(SearchTag)) continue;
		if ()

	}
	
	
	
}	


