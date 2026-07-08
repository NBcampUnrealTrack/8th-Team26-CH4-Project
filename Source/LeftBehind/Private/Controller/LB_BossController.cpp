// Fill out your copyright notice in the Description page of Project Settings.


#include "Controller/LB_BossController.h"

#include "BehaviorTree/BehaviorTree.h"
#include "Characters/Boss/LB_BossCharacter.h"
#include "Controller/Component/LB_AttackPatternComponent.h"
#include "Controller/Component/LB_ThreatComponent.h"


// Sets default values
ALB_BossController::ALB_BossController()
{
	UE_LOG(LogTemp, Warning, TEXT("ALB-BossContorller Activate"));
	PrimaryActorTick.bCanEverTick = false;
	
	AttackPatternComponent = CreateDefaultSubobject<ULB_AttackPatternComponent>(TEXT("AttackPatternComponent"));
	
	ThreatComponent = CreateDefaultSubobject<ULB_ThreatComponent>(TEXT("ThreatComponent"));
	
}

void ALB_BossController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	ALB_BossCharacter* Boss = Cast<ALB_BossCharacter>(InPawn);
	if (!Boss) return;
	
	if (IsValid(BehaviorTree))
	{
		UE_LOG(LogTemp, Warning, TEXT("BehabviorTree Activate"));
		RunBehaviorTree(BehaviorTree);
	}
	
	
}


