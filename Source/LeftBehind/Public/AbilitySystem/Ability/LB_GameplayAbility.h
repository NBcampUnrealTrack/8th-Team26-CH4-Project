// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LB_AbilityTypes.h"
#include "Abilities/GameplayAbility.h"
#include "LB_GameplayAbility.generated.h"

class UNiagaraSystem;
/**
 * 
 */
UCLASS()
class LEFTBEHIND_API ULB_GameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LeftBehind|Debug")
	bool bDrawDebug = false;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LeftBehind|Effect")
	UNiagaraSystem* AbilityEffect;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LeftBehind|Sound")
	USoundBase* Sound;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LeftBehind|Sound")
	USoundBase* HitSound;	
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LeftBehind|Attack")
	FLB_AttackConfig AttackConfiguration;
	
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	
	virtual void SendDamageforServer(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData);
	
	virtual void PlayEffect(AActor* PlayActor, FVector PlayLocation, UNiagaraSystem* AbilityEffect );
	
	virtual void PlaySound(AActor* PlayActor, FVector PlayLocation, USoundBase* SoundBase );
	

private:
	
	
	
};
