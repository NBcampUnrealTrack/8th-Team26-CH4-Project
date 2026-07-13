// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Task/LB_TelegraphAbilityTask.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayAnimAndWait.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "BehaviorTree/BehaviorTreeTypes.h"

ULB_TelegraphAbilityTask* ULB_TelegraphAbilityTask::PlayTelegraph(UGameplayAbility* OwningAbility, UAnimMontage* TelegraphPlayMontage,
                                             FGameplayTag TelegraphStateTag, FGameplayTag TelegraphCueTag)
{
	ULB_TelegraphAbilityTask* TelegraphAbilityTask = NewAbilityTask<ULB_TelegraphAbilityTask>(OwningAbility);
	
	TelegraphAbilityTask->PlayMontage = TelegraphPlayMontage;
	TelegraphAbilityTask->StateTag = TelegraphStateTag;
	TelegraphAbilityTask->CueTag = TelegraphCueTag;
	
	return TelegraphAbilityTask;
	
}

void ULB_TelegraphAbilityTask::Activate()
{
	Super::Activate();
	
	if (!IsValid(PlayMontage))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LB TelegraphTask] PlayMontage is Null"));
		OnTaskCancelled.Broadcast();
		EndTask();
		return;
	}
	if (UAbilitySystemComponent* ASC = AbilitySystemComponent.Get())
	{
		if (StateTag.IsValid())
		{
			ASC->AddLooseGameplayTag(StateTag);
		}
		
		if (CueTag.IsValid())
		{
			FGameplayCueParameters CueParams;
			if (const AActor* AvavActor = ASC->GetAvatarActor())
			{
				CueParams.Location = AvavActor->GetActorLocation();
				CueParams.Normal = AvavActor->GetActorForwardVector();
				
			}
			ASC->ExecuteGameplayCue(CueTag,CueParams);
		}
	}
	
	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(Ability,NAME_None,PlayMontage,1.0f);
	MontageTask->OnCompleted.AddDynamic(this, &ULB_TelegraphAbilityTask::HandleComplete);
	MontageTask->OnCancelled.AddDynamic(this, &ULB_TelegraphAbilityTask::HandleCancelled);
	MontageTask->OnInterrupted.AddDynamic(this, &ULB_TelegraphAbilityTask::HandleCancelled);
	MontageTask->ReadyForActivation();
	
	
}

void ULB_TelegraphAbilityTask::OnDestroy(bool bInOwnerFinished)
{
	
	RemoveTag();
	Super::OnDestroy(bInOwnerFinished);
	
}


void ULB_TelegraphAbilityTask::HandleComplete()
{
	RemoveTag();
	OnTaskCompleted.Broadcast();
	EndTask();
}

void ULB_TelegraphAbilityTask::HandleCancelled()
{
	RemoveTag();
	OnTaskCancelled.Broadcast();
	EndTask();
}

void ULB_TelegraphAbilityTask::RemoveTag() const
{
	if (UAbilitySystemComponent* ASC = AbilitySystemComponent.Get())
	{
		if (StateTag.IsValid())
		{
			ASC->RemoveLooseGameplayTag(StateTag);
		}
	}
}
