// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "LB_BossController.generated.h"

class ULB_BossAIBlackBoard;
class ULB_AttackPatternComponent;
class ULB_ThreatComponent;

UCLASS()
class LEFTBEHIND_API ALB_BossController : public AAIController
{
	GENERATED_BODY()

public:

	ALB_BossController();

	virtual void OnPossess(APawn* InPawn) override;
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly,Category = "BOSS|AI")
	TObjectPtr<ULB_ThreatComponent> ThreatComponent;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly,Category = "BOSS|AI")
	TObjectPtr<ULB_AttackPatternComponent> AttackPatternComponent;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly,Category = "BOSS|AI", meta=(AllowPrivateAccess = true))
	TObjectPtr<ULB_BossAIBlackBoard> BossAIBlackBoard;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly,Category = "BOSS|AI", meta=(AllowPrivateAccess = true))
	TObjectPtr<UBehaviorTree> BehaviorTree;
};
