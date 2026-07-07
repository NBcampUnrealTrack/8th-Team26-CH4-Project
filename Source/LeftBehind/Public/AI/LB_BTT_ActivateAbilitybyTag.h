// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "BehaviorTree/BTTaskNode.h"
#include "LB_BTT_ActivateAbilitybyTag.generated.h"

class UAbilitySystemComponent;
/**
 * 
 */
UCLASS()
class LEFTBEHIND_API ULB_BTT_ActivateAbilitybyTag : public UBTTaskNode
{
	GENERATED_BODY()
	
public:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory);
	
	UPROPERTY(EditAnywhere, Category = "Ability")
	FGameplayTag AbilityTag;
	
protected:
	virtual void OnAbilityEnded(const FAbilityEndedData& EndedData);
	
private:
	FGameplayAbilitySpec* FindSpecByTag(UAbilitySystemComponent* ASC, const FGameplayTag& Tag);
	
	
	TWeakObjectPtr<UBehaviorTreeComponent> CachedOwnerComp;
	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;
	FGameplayAbilitySpecHandle ActivatedSpecHandle;
	
};
