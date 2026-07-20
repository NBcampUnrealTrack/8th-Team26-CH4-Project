// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/LB_BTT_MoveToTarget.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Characters/Boss/LB_BossCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

EBTNodeResult::Type ULB_BTT_MoveToTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::ExecuteTask(OwnerComp, NodeMemory);
	
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB) return EBTNodeResult::Failed;
	
	AActor* Target = Cast<AActor>(BB->GetValueAsObject(FName("Target")));
	if (!Target) return EBTNodeResult::Failed;
	
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController) return EBTNodeResult::Failed;
	
	ALB_BaseCharacter* BaseCharacter = Cast<ALB_BaseCharacter>(OwnerComp.GetAIOwner()->GetPawn());
	if (!IsValid(BaseCharacter)) return EBTNodeResult::Failed;
	
	BB->SetValueAsFloat(FName("AttackRange"),BaseCharacter->MeleeDistance);
	BB->SetValueAsFloat(FName("WideAttackRange"),BaseCharacter->WideAttackTrigger);
	
	/*AIController->SetFocus(Target,EAIFocusPriority::Gameplay);*/
	AIController->SetFocalPoint(Target->GetActorLocation(),EAIFocusPriority::Gameplay);
	

	
	
	return EBTNodeResult::Succeeded;
}
