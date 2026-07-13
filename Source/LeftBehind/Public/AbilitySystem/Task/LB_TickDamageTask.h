// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilitySystem/Ability/Enemy/LB_BossChargeAbility.h"
#include "LB_TickDamageTask.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTickDamageResultDelegate);

UCLASS()
class LEFTBEHIND_API ULB_TickDamageTask : public UAbilityTask
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks")
	static ULB_TickDamageTask* CreateTickDamageTask(
	UGameplayAbility* OwningAbility, 
	FVector TargetLocation,
	 float ChargingSpeed, 
	 float MaxDuration, 
	 FLB_AttackConfig AttackConfig);
	
	
	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
	virtual void OnDestroy(bool bInOwnerFinished) override;
	
	
	void HandleDamageableActorsInHitBox();
	
	UPROPERTY(BlueprintAssignable)
	FTickDamageResultDelegate OnTargetImpact;
	
	UPROPERTY(BlueprintAssignable)
	FTickDamageResultDelegate OnObstacleHit;
	
	UPROPERTY(BlueprintAssignable)
	FTickDamageResultDelegate OnTaskCompleted;
	
	UPROPERTY(BlueprintAssignable)
	FTickDamageResultDelegate OnTimeOut;
	
protected:
	
private:
	//돌진 속도
	float TaskCharingSpeed;
	//돌진 최대 시간
	float TaskMaxDuration;
	//현재 시간
	float CurrentTime;
	//도착 거리와 현재 위치의 성공 조건 체크를 위한 Margin
	float MarginDistance;
	//도착 위치
	FVector Destination;
	//현재 위치
	FVector StartLocation;
	

	FLB_AttackConfig AttackConfig;
	
};
