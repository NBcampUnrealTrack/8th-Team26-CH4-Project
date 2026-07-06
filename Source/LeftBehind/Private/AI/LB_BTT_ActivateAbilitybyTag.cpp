// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/LB_BTT_ActivateAbilitybyTag.h"

#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Characters/Boss/LB_BossCharacter.h"
#include "EntitySystem/MovieSceneEntitySystemRunner.h"

EBTNodeResult::Type ULB_BTT_ActivateAbilitybyTag::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::ExecuteTask(OwnerComp, NodeMemory);
	
	if (!AbilityTag.IsValid()) return EBTNodeResult::Failed; 
	
	ALB_BossCharacter* BossCharacter = Cast<ALB_BossCharacter>(OwnerComp.GetAIOwner()->GetPawn());
	if (!IsValid(BossCharacter)) return EBTNodeResult::Failed;
	
	UAbilitySystemComponent* ASC = BossCharacter->GetAbilitySystemComponent();
	if (ASC == nullptr) return EBTNodeResult::Failed;
	
	
	FGameplayAbilitySpec* Spec = FindSpecByTag(ASC, AbilityTag);
	if (!Spec) return EBTNodeResult::Failed;
	
	//TryActiavteAbility를 통해서 Spec를 통해서 능력을 발동시킨다.
	bool bActivated = ASC->TryActivateAbility(Spec->Handle);
	if (!bActivated) return EBTNodeResult::Failed;
	
	
	//다른 함수들에서 쓰일 캐쉬 변수
	CachedASC = ASC;
	CachedOwnerComp = &OwnerComp;
	ActivatedSpecHandle = Spec->Handle;
	
	//능력이 끝날 때의 델리게이트를 구독한다.
	ASC->OnAbilityEnded.AddUObject(this,&ULB_BTT_ActivateAbilitybyTag::OnAbilityEnded);
	
	return  EBTNodeResult::InProgress;
	
	
	
	
	
}

//TaskFinished는 성공/실패/ 강제중단 시에 공통적으로 정리하기 위해서이다.
void ULB_BTT_ActivateAbilitybyTag::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory,
	EBTNodeResult::Type TaskResult)
{
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
	
	//델리게이트 구독을 해제ㅔ한다.
	if (CachedASC.IsValid())
	{
		CachedASC->OnAbilityEnded.RemoveAll(this);
	}
	
	CachedOwnerComp.Reset();
	CachedASC.Reset();
	ActivatedSpecHandle = FGameplayAbilitySpecHandle();
	
}

//상위 노드에서 강제적으로 종료될 경우, Ability 또한 강제로 종료한다.
EBTNodeResult::Type ULB_BTT_ActivateAbilitybyTag::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	
	if (CachedASC.IsValid() && ActivatedSpecHandle.IsValid())
	{
		FGameplayAbilitySpec* Spec = CachedASC->FindAbilitySpecFromHandle(ActivatedSpecHandle);
		if (Spec && Spec->IsActive())
		{
			CachedASC->CancelAbilityHandle(ActivatedSpecHandle);
		}
	}
	
	return EBTNodeResult::Aborted;
}



void ULB_BTT_ActivateAbilitybyTag::OnAbilityEnded(const FAbilityEndedData& EndedData)
{
	//발동한 능력에 맞지 않는 Handle일 때, BT가 전혀 맞지 않을 때 발동시키지 않는다.
	if (EndedData.AbilitySpecHandle != ActivatedSpecHandle) return;
	if (!CachedOwnerComp.IsValid()) return;
	
	//캔슬 여부에 따라서, 실패 혹은 성공을 만든다.
	EBTNodeResult::Type Result = EndedData.bWasCancelled ? EBTNodeResult::Failed : EBTNodeResult::Succeeded;
	
	
	FinishLatentTask(*CachedOwnerComp,Result);
}

//발동한 능력에 맞는 Spec을 Tag로 탐색한다.
FGameplayAbilitySpec* ULB_BTT_ActivateAbilitybyTag::FindSpecByTag(UAbilitySystemComponent* ASC, const FGameplayTag& Tag)
{
	for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->GetAssetTags().HasTag(Tag))
		{
			return &Spec;
		}
	}
	return nullptr;
}
