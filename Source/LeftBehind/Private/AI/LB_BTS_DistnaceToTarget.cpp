// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/LB_BTS_DistnaceToTarget.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"

void ULB_BTS_DistnaceToTarget::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
	
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB) return;

	AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetKey.SelectedKeyName));
	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* SelfPawn = AIController ? AIController->GetPawn() : nullptr;
	
	if (!Target || !SelfPawn) return;
	
	float Distance =
	FVector::Distance(
		SelfPawn->GetActorLocation(),
		Target->GetActorLocation());
	
	BB->SetValueAsFloat(TargetLocationKey.SelectedKeyName, Distance);
}
