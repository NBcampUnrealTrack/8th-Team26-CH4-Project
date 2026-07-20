// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/LB_BTD_Distance.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"

bool ULB_BTD_Distance::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB) return false;

	AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetKey.SelectedKeyName));
	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* SelfPawn = AIController ? AIController->GetPawn() : nullptr;
	
	if (!Target || !SelfPawn) return false;
	
	float Distance =
	FVector::Distance(
		SelfPawn->GetActorLocation(),
		Target->GetActorLocation());
	

	return Distance >= Range;


}
