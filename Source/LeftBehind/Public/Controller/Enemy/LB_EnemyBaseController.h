// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/AIModule/Classes/AIController.h"
#include "LB_EnemyBaseController.generated.h"

UCLASS()
class LEFTBEHIND_API ALB_EnemyBaseController : public AAIController
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ALB_EnemyBaseController();

private:
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "BOSS|AI",meta=(AllowPrivateAccess=true))
	TObjectPtr<UBlackboardComponent> BlackboardComponent;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "BOSS|AI",meta=(AllowPrivateAccess=true))
	TObjectPtr<UBehaviorTree> BehaviorTree;

	
	
};
