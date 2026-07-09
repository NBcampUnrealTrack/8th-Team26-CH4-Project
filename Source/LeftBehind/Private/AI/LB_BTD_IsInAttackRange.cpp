// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/LB_BTD_IsInAttackRange.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"

//Target과 자기 자신의 거리를 측정하기 위한 함수
bool ULB_BTD_IsInAttackRange::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB) return false;

	AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetKey.SelectedKeyName));
	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* SelfPawn = AIController ? AIController->GetPawn() : nullptr;

	if (!Target || !SelfPawn) return false;

	const float Range = BB->GetValueAsFloat(RangeKey.SelectedKeyName) + Margin;
	const float DistSq = FVector::DistSquared(Target->GetActorLocation(), SelfPawn->GetActorLocation());

	return DistSq <= FMath::Square(Range);
	
	
}
