// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Ability/LB_GameplayAbility.h"
#include "LB_BossPhaseTransitied.generated.h"

/**
 * 
 */
UCLASS()
class LEFTBEHIND_API ULB_BossPhaseTransitied : public ULB_GameplayAbility
{
	GENERATED_BODY()
	
public:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	
	UPROPERTY(EditDefaultsOnly, Category="Stun")
	TSubclassOf<UGameplayEffect> StunGEClass;

	UPROPERTY(EditDefaultsOnly, Category="Stun")
	UAnimMontage* StunMontage;

	UPROPERTY(EditDefaultsOnly, Category="Stun")
	float PhaseTransitionDuration = 3.f;

	UPROPERTY(EditDefaultsOnly, Category="Phase")
	FGameplayTag OldPhaseTag;

	UPROPERTY(EditDefaultsOnly, Category="Phase")
	FGameplayTag NewPhaseTag;

	UFUNCTION()
	void OnStunTagRemoved();

	UFUNCTION()
	void OnStunMontageFinished();
};
