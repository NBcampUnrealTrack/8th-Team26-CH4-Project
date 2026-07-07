// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "GameplayAbilitySpecHandle.h"
#include "BehaviorTree/BTTaskNode.h"
#include "LB_BTT_SelectAttack.generated.h"

class UAbilitySystemComponent;
struct FAbilityEndedData;
/**
 * 
 */
UCLASS()
class LEFTBEHIND_API ULB_BTT_SelectAttack : public UBTTaskNode
{
	GENERATED_BODY()
	
public:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory);

	
protected:
	virtual void OnAbilityEnded(const FAbilityEndedData& EndedData);
	
private:
	FGameplayAbilitySpec* FindSpecByTag(UAbilitySystemComponent* ASC, const FGameplayTag& Tag);
	
	
	TWeakObjectPtr<UBehaviorTreeComponent> CachedOwnerComp;
	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;
	FGameplayAbilitySpecHandle ActivatedSpecHandle;
	
};
