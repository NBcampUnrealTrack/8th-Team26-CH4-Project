// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Controller/Component/LB_AttackPatternComponent.h"
#include "Controller/Component/LB_ThreatComponent.h"
#include "Runtime/AIModule/Classes/AIController.h"
#include "LB_EnemyBaseController.generated.h"

class ULB_AbilitySystemComponent;

UCLASS()
class LEFTBEHIND_API ALB_EnemyBaseController : public AAIController
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ALB_EnemyBaseController();
	
	virtual void OnPossess(APawn* InPawn) override;
	
	virtual void BeginPlay() override;

private:
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "Enemy|AI",meta=(AllowPrivateAccess=true))
	TObjectPtr<UBlackboardComponent> BlackboardComponent;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "Enemy|AI",meta=(AllowPrivateAccess=true))
	TObjectPtr<UBehaviorTree> BehaviorTree;

	
	
};
