// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "LB_BTT_SelectClosetTarget.generated.h"

/**
 * 
 */
UCLASS()
class LEFTBEHIND_API ULB_BTT_SelectClosetTarget : public UBTTaskNode
{
	GENERATED_BODY()
	
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
