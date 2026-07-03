// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Characters/Boss/LB_BossCharacter.h"
#include "LB_BossAIBlackBoard.generated.h"


class ALB_BaseCharacter;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LEFTBEHIND_API ULB_BossAIBlackBoard : public UBlackboardComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	ULB_BossAIBlackBoard();


private:
	ALB_BaseCharacter* Target;
	
	FVector TargetLocation;
	
	FPhaseInfo CurrentPhase;
	
	
};
