// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/LB_BTT_SelectClosetTarget.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Characters/LB_EnemyCharacter.h"
#include "Controller/Component/LB_ThreatComponent.h"

EBTNodeResult::Type ULB_BTT_SelectClosetTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AICon = OwnerComp.GetAIOwner();
	if (!IsValid(AICon)) return EBTNodeResult::Failed;
	
	ALB_EnemyCharacter* EnemyCharacter = Cast<ALB_EnemyCharacter>(OwnerComp.GetAIOwner()->GetPawn());
	if (!IsValid(EnemyCharacter)) return EBTNodeResult::Failed;
	
	UAbilitySystemComponent* ASC = EnemyCharacter->GetAbilitySystemComponent();
	if (ASC == nullptr) return EBTNodeResult::Failed;
	
	AActor* Target = EnemyCharacter->GetThreatComponent()->ClosestPlayerCharacter();
	
	if (!Target) return EBTNodeResult::Failed;
	
	if (UBlackboardComponent* BlackboardComponent = OwnerComp.GetBlackboardComponent())
	{
		BlackboardComponent->SetValueAsObject(FName("Target"),Target);

		
		/*UE_LOG(LogTemp, Warning,
	TEXT("BB Target : %s"),
	*GetNameSafe(Cast<AActor>(BlackboardComponent->GetValueAsObject(TEXT("Target")))));*/
	}
	AICon->SetFocus(Target, EAIFocusPriority::Gameplay);
	


	/*UE_LOG(LogTemp, Warning, TEXT("Focus=%s"),
		*GetNameSafe(AICon->GetFocusActor()));

	UE_LOG(LogTemp, Warning, TEXT("ControlRot=%s"),
		*AICon->GetControlRotation().ToString());*/

	
	
	return EBTNodeResult::Succeeded;
	
	
}
