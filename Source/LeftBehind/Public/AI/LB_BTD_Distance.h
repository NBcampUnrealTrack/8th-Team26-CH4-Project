// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "LB_BTD_Distance.generated.h"

/**
 * 
 */
UCLASS()
class LEFTBEHIND_API ULB_BTD_Distance : public UBTDecorator
{
	GENERATED_BODY()
	
public:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
	
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetKey;
	
	
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	float Range;
};
