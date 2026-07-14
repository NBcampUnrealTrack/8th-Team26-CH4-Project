// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "LB_TelegraphAbilityTask.generated.h"


class UAbilityTask_PlayMontageAndWait;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTelegraphDelegate);
/**
 * 
 */
UCLASS()
class LEFTBEHIND_API ULB_TelegraphAbilityTask : public UAbilityTask
{
	GENERATED_BODY()
	
public:
	
	//Ability에서 임의 위치에 생성하기 위해서 만들어진 함수
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks")
	static ULB_TelegraphAbilityTask* PlayTelegraph(
		UGameplayAbility* OwningAbility, 
		UAnimMontage* TelegraphPlayMontage, 
		FGameplayTag TelegraphStateTag,
		FGameplayTag TelegraphCueTag);
	
	
	
	virtual void Activate() override;
	
	virtual void OnDestroy(bool bInOwnerFinished) override;
	
	//Task가 종료되었을 때를 대비한 델리게이트
	UPROPERTY(BlueprintAssignable)
	FTelegraphDelegate OnTaskCancelled;
	
	//Task가 완성되었을 때를 대비한 델리게이트
	UPROPERTY(BlueprintAssignable)
	FTelegraphDelegate OnTaskCompleted;
	
	
	
protected:
	
	//PlayTelegraph를 통해서 내부에 저장될 Montag, Tag
	UPROPERTY()
	TObjectPtr<UAnimMontage> PlayMontage;
	
	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;
	
	FGameplayTag StateTag;
	FGameplayTag CueTag;
	
	//취소, 완성을 대비하기 위한 함수
	UFUNCTION()
	void HandleComplete();
	
	UFUNCTION()
	void HandleCancelled();
	
private:
	
	//Telegraph 제거를 위한 함수
	void RemoveTag() const;
	
	
	
	
};
