// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "LB_BTD_IsInAttackRange.generated.h"

/**
 * 
 */
UCLASS()
class LEFTBEHIND_API ULB_BTD_IsInAttackRange : public UBTDecorator
{
	GENERATED_BODY()
	
public:
	//Target과 자기 자신의 거리를 측정하기 위한 함수
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
	
	//목표 설정을 위한 변수
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetKey;

	//거리 조절을 위한 함수
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector RangeKey;
	
	UPROPERTY(EditAnywhere, Category = "Range")
	float Margin = 300.f;
	
};
