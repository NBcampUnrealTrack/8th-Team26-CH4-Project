// Fill out your copyright notice in the Description page of Project Settings.


#include "Controller/Enemy/LB_EnemyBaseController.h"

#include "BehaviorTree/BehaviorTree.h"
#include "Controller/Component/LB_AttackPatternComponent.h"
#include "Controller/Component/LB_ThreatComponent.h"
#include "GameplayTags/LBTags.h"


// Sets default values
ALB_EnemyBaseController::ALB_EnemyBaseController()
{
	
	PrimaryActorTick.bCanEverTick = false;
}

void ALB_EnemyBaseController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	
	if (IsValid(BehaviorTree))
	{
		UE_LOG(LogTemp, Warning, TEXT("BehabviorTree Activate"));
		RunBehaviorTree(BehaviorTree);
	}
	
}

void ALB_EnemyBaseController::BeginPlay()
{
	Super::BeginPlay();
	
	
}


