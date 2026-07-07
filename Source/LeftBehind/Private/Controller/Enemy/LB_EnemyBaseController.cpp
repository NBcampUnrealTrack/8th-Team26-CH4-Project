// Fill out your copyright notice in the Description page of Project Settings.


#include "Controller/Enemy/LB_EnemyBaseController.h"
#include "Controller/Component/LB_AttackPatternComponent.h"
#include "Controller/Component/LB_ThreatComponent.h"
#include "GameplayTags/LBTags.h"


// Sets default values
ALB_EnemyBaseController::ALB_EnemyBaseController()
{
	
	PrimaryActorTick.bCanEverTick = false;
	
	AttackPatternComponent = CreateDefaultSubobject<ULB_AttackPatternComponent>(TEXT("AttackPatternComponent"));
	
	ThreatComponent = CreateDefaultSubobject<ULB_ThreatComponent>(TEXT("ThreatComponent"));
	
	
	
	
}

void ALB_EnemyBaseController::BeginPlay()
{
	Super::BeginPlay();
	
	
}


