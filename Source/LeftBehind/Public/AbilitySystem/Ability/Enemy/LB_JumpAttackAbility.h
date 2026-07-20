// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/LB_GameplayAbility.h"
#include "LB_JumpAttackAbility.generated.h"

class ALB_TelegraphIndicator;
/**
 * 
 */
UCLASS()
class LEFTBEHIND_API ULB_JumpAttackAbility : public ULB_GameplayAbility
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="LB|JumpAttack")
	FLB_RootMotionJumpForceParams JumpAttackConfiguration;
	
	ULB_JumpAttackAbility();
	
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UFUNCTION()
	virtual void OnTelegraphFinished();
	
	UFUNCTION()
	virtual void OnJumpAttackFinished();
	
	UFUNCTION()
	virtual void OnAbilityFinished();
	
	UFUNCTION()
	virtual void OnAbilityCancelled();
	void ClearIndicator();


	/*FName TaskInstanceName, FRotator Rotation, float Distance, float Height, float Duration, float MinimumLandedTriggerTime,
	bool bFinishOnLanded, ERootMotionFinishVelocityMode VelocityOnFinishMode, FVector SetVelocityOnFinish, float ClampVelocityOnFinish, UCurveVector* PathOffsetCurve, UCurveFloat* TimeMappingCurve*/

private:
	
	
	//---Telegraph---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Telegraph",meta=(AllowPrivateAccess = true))
	bool bIsTelegraph;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Telegraph",meta=(AllowPrivateAccess = true))
	TObjectPtr<UAnimMontage> TelegraphMontage;
	
	UPROPERTY()
	TArray<ALB_TelegraphIndicator*> SpawnedIndicators;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly, Category="LB|Telegraph", meta=(AllowPrivateAccess = true))
	TSubclassOf<ALB_TelegraphIndicator> IndicatorClass;
	
	UPROPERTY(EditDefaultsOnly, Category="LB|Telegraph",meta=(AllowPrivateAccess = true))
	float IndicatorRadius;
	
	UPROPERTY(EditDefaultsOnly, Category="LB|Telegraph",meta=(AllowPrivateAccess = true))
	float IndicatorLength;
	
	
	
	const FGameplayEventData* CachedTriggerEventData;
	
};
