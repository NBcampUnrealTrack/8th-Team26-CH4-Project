// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/LB_BTT_MoveToTarget.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Characters/Boss/LB_BossCharacter.h"

EBTNodeResult::Type ULB_BTT_MoveToTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::ExecuteTask(OwnerComp, NodeMemory);
	
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB) return EBTNodeResult::Failed;
	
	AActor* Target = Cast<AActor>(BB->GetValueAsObject(FName("Target")));
	if (!Target) return EBTNodeResult::Failed;
	
	ALB_BossCharacter* BossCharacter = Cast<ALB_BossCharacter>(OwnerComp.GetAIOwner()->GetPawn());
	if (!IsValid(BossCharacter)) return EBTNodeResult::Failed;
	
	BB->SetValueAsFloat(FName("AttackRange"),BossCharacter->MeleeDistance);
	
	return EBTNodeResult::Succeeded;
}
