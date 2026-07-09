// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/LB_BTT_SelectTarget.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Characters/Boss/LB_BossCharacter.h"
#include "Controller/Component/LB_ThreatComponent.h"

EBTNodeResult::Type ULB_BTT_SelectTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	
	
	ALB_BossCharacter* BossCharacter = Cast<ALB_BossCharacter>(OwnerComp.GetAIOwner()->GetPawn());
	if (!IsValid(BossCharacter)) return EBTNodeResult::Failed;
	
	UAbilitySystemComponent* ASC = BossCharacter->GetAbilitySystemComponent();
	if (ASC == nullptr) return EBTNodeResult::Failed;
	
	AActor* Target = BossCharacter->GetThreatComponent()->SelectMostThreatCharacter();
	
	if (!Target) return EBTNodeResult::Failed;
	
	if (UBlackboardComponent* BlackboardComponent = OwnerComp.GetBlackboardComponent())
	{
		BlackboardComponent->SetValueAsObject(FName("Target"),Target);
		
		/*UE_LOG(LogTemp, Warning,
	TEXT("BB Target : %s"),
	*GetNameSafe(Cast<AActor>(BlackboardComponent->GetValueAsObject(TEXT("Target")))));*/
	}
	
	
	
	return EBTNodeResult::Succeeded;
}
