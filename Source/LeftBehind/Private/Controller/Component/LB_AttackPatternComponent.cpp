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

//가능한 능력을 선택한다.
FGameplayAbilitySpecHandle ULB_AttackPatternComponent::SelectAttackPattern()
{

	CandidateSkillList.Empty();
	
	if (!CacahedASC.IsValid()) return FGameplayAbilitySpecHandle(); // invalid handle 반환

		

	
	FGameplayTag SearchTag = FGameplayTag::RequestGameplayTag(TEXT("LBTags.Abilities"));
	
	
	for (FGameplayAbilitySpec& Spec : CacahedASC->GetActivatableAbilities())
	{
		// UE 5.7부터 AbilityTags 직접 접근이 폐기 예정이므로 읽기 전용 공식 API를 사용한다.
		if (!Spec.Ability->GetAssetTags().HasTag(SearchTag)) continue;
		if (!Spec.Ability->CanActivateAbility(Spec.Handle, CacahedASC->AbilityActorInfo.Get())) continue;
		
		
		
		CandidateSkillList.Add(Spec.Handle);

	}
	
	if (CandidateSkillList.IsEmpty()) return FGameplayAbilitySpecHandle();
	
	return CandidateSkillList[0];
	
}

void ULB_AttackPatternComponent::BeginPlay()
{
	Super::BeginPlay();
	
	
	ALB_BossController* BossController=  Cast<ALB_BossController>(GetOwner());
	if (!BossController) return;
	
	ALB_BossCharacter* BossCharacter = Cast<ALB_BossCharacter>(BossController->GetPawn());
	if (!BossCharacter) return;
	
	CacahedASC = Cast<ULB_AbilitySystemComponent>(BossCharacter->GetAbilitySystemComponent());
	
}	


