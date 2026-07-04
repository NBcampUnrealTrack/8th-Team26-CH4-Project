// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/LB_GameplayAbility.h"
#include "LB_BossGameplayAbility.generated.h"

/**
 * 
 */
UCLASS()
class LEFTBEHIND_API ULB_BossGameplayAbility : public ULB_GameplayAbility
{
	GENERATED_BODY()
	
public:
	virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility) override;
	
	
};
