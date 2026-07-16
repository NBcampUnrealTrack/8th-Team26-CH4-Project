// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilitySystem/Ability/LB_AbilityTypes.h"
#include "LB_JumSmash.generated.h"

/**
 * 
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTaskResultDelegate);

UCLASS()
class LEFTBEHIND_API ULB_JumSmash : public UAbilityTask
{
	GENERATED_BODY()
public:
	virtual void Activate() override;
	virtual void OnDestroy(bool bInOwnerFinished) override;
	virtual void TickTask(float DeltaTime) override;
	
	FTaskResultDelegate OnCompleted;
	FTaskResultDelegate OnCancelled;
private:
	FLB_AttackConfig AttackConfig;
	
};
