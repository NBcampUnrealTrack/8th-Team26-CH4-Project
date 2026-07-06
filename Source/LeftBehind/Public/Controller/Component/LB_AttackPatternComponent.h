// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LB_AttackPatternComponent.generated.h"


struct FGameplayAbilitySpecHandle;
class ULB_AbilitySystemComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LEFTBEHIND_API ULB_AttackPatternComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	ULB_AttackPatternComponent();
	
	FGameplayAbilitySpecHandle SelectAttackPattern();
	
	virtual void BeginPlay() override;
private:
	
	TWeakObjectPtr<ULB_AbilitySystemComponent> CacahedASC;
	
	TArray<FGameplayAbilitySpecHandle> CandidateSkillList;
	
	
	
	
};
