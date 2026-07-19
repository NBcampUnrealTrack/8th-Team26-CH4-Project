// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Ability/Enemy/LB_JumpAttackAbility.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionJumpForce.h"
#include "Abilities/Tasks/AbilityTask_VisualizeTargeting.h"
#include "AbilitySystem/Ability/LB_TelegraphIndicator.h"
#include "AbilitySystem/Task/LB_TelegraphAbilityTask.h"
#include "GameFramework/RootMotionSource.h"
#include "GameplayTags/LBTags.h"

ULB_JumpAttackAbility::ULB_JumpAttackAbility()
{
	// 데미지 계산은 서버만 수행한다. 클라이언트가 직접 HP를 바꾸면 멀티플레이에서 치트와 불일치가 생긴다.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void ULB_JumpAttackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	if (!AvatarActor->HasAuthority())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	CachedTriggerEventData = TriggerEventData;
	
	//전조 증상 여부에 따른 Task 생성 추가
	if (bIsTelegraph)
	{

		
		if (IndicatorClass)
		{
			FActorSpawnParameters IndicatorSpawnParams;
			FVector SpawnLocation = AvatarActor->GetActorLocation() + AvatarActor->GetActorForwardVector()*JumpAttackConfiguration.Distance;
			SpawnLocation.Z = 0;
			FRotator SpawnRotation = AvatarActor->GetActorRotation();
			if (ALB_TelegraphIndicator* Indicator = GetWorld()->SpawnActor<ALB_TelegraphIndicator>(
				IndicatorClass, SpawnLocation, SpawnRotation, IndicatorSpawnParams))
			{
				Indicator->SetAsCircle(IndicatorRadius);
				SpawnedIndicators.Add(Indicator);
			}
		}
		
		ULB_TelegraphAbilityTask* TelegraphAbilityTask = ULB_TelegraphAbilityTask::PlayTelegraph(
			this,
			TelegraphMontage,
			LBTags::LBAbilities::Enemy::Telegraph,
			LBTags::LBCues::Enemy::TelegraphCue);
		if (IsValid(TelegraphAbilityTask))
		{
			TelegraphAbilityTask->OnTaskCompleted.AddDynamic(this,&ULB_JumpAttackAbility::OnTelegraphFinished);
			TelegraphAbilityTask->OnTaskCancelled.AddDynamic(this,&ULB_JumpAttackAbility::OnAbilityCancelled);
			TelegraphAbilityTask->ReadyForActivation();
		}
		

	}
	else
	{
		OnTelegraphFinished();
	}
}

void ULB_JumpAttackAbility::OnTelegraphFinished()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor))
	{
		EndAbility(GetCurrentAbilitySpecHandle()
		,GetCurrentActorInfo()
		,GetCurrentActivationInfo()
		,true
		,true);
		return;
	}
	
	UAbilityTask_ApplyRootMotionJumpForce* JumpAttack = UAbilityTask_ApplyRootMotionJumpForce::ApplyRootMotionJumpForce(
		this,
		FName("JumpAttack"),
		AvatarActor->GetActorRotation(),
		JumpAttackConfiguration.Distance,
		JumpAttackConfiguration.Height,
		JumpAttackConfiguration.Duration,
		0.5f,
		JumpAttackConfiguration.bFinishOnLanded,
		ERootMotionFinishVelocityMode::SetVelocity,   
		FVector::ZeroVector,                          
		 0.f,                                          
		JumpAttackConfiguration.PathOffsetCurve,                                    
		JumpAttackConfiguration.TimeMappingCurve  
		
		);
	
	if (IsValid(JumpAttack))
	{
		JumpAttack->OnFinish.AddDynamic(this,&ULB_JumpAttackAbility::OnJumpAttackFinished);
		JumpAttack->OnLanded.AddDynamic(this,&ULB_JumpAttackAbility::OnJumpAttackFinished);
		JumpAttack->ReadyForActivation();
	}
	

}

void ULB_JumpAttackAbility::OnJumpAttackFinished()
{
	SendDamageforServer(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo(),GetCurrentActivationInfo(),CachedTriggerEventData);
	PlayEffect(GetAvatarActorFromActorInfo(),GetAvatarActorFromActorInfo()->GetActorLocation(),AbilityEffect);
	PlaySound(GetAvatarActorFromActorInfo(),GetAvatarActorFromActorInfo()->GetActorLocation(),Sound);
	
	
	OnAbilityFinished();
}

void ULB_JumpAttackAbility::OnAbilityFinished()
{
	
	ClearIndicator();
	EndAbility(GetCurrentAbilitySpecHandle()
	,GetCurrentActorInfo()
	,GetCurrentActivationInfo()
	,true
	,false);
}

void ULB_JumpAttackAbility::OnAbilityCancelled()
{
	EndAbility(GetCurrentAbilitySpecHandle()
	,GetCurrentActorInfo()
	,GetCurrentActivationInfo()
	,true
	,true);
}

void ULB_JumpAttackAbility::ClearIndicator()
{

	for (ALB_TelegraphIndicator* Indicator : SpawnedIndicators)
	{
		if (IsValid(Indicator))
		{
			Indicator->Destroy();
		}
	}
	
	SpawnedIndicators.Empty();
}


