// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/LB_GameplayAbility.h"
#include "AbilitySystem/Ability/LB_AbilityTypes.h"
#include "LB_SummonAbility.generated.h"

class ALB_TelegraphIndicator;
class ALB_EnemyCharacter;
/**
 * 
 */
UCLASS()
class LEFTBEHIND_API ULB_SummonAbility : public ULB_GameplayAbility
{
	GENERATED_BODY()
	
public:
	
	ULB_SummonAbility();

	
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	UFUNCTION()
	virtual void OnAbilityActivated();
	
	UFUNCTION()
	virtual void OnAbilityCancelled();
	
	
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,Category = "LB|Summon")
	FLB_SummonSpawnParams SummonSpawnParams;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,Category = "LB|Ability")
	bool bIsTelegraph = false;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,Category = "LB|Ability")
	TObjectPtr<UAnimMontage> TelegraphMontage;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,Category = "LB|Ability")
	TObjectPtr<UAnimMontage> PrimaryMontage;
	
	
	// ---Indicator---
	UPROPERTY()
	TArray<FVector> PendingSpawnLocations;

	UPROPERTY()
	TArray<ALB_TelegraphIndicator*> SpawnedIndicators;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly, Category="LB|Summon")
	TSubclassOf<ALB_TelegraphIndicator> IndicatorClass;
	
	UPROPERTY(EditDefaultsOnly, Category="LB|Summon")
	float IndicatorRadius = 150.f;
	//소환 직후부터 나오기 까지의 시간
	UPROPERTY(EditDefaultsOnly, Category="LB|Summon")
	float SummonCastingTime;
	
	void ClearIndicator();
};
