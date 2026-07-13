// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Ability/Enemy/LB_BossChargeAbility.h"

#include "AbilitySystem/Task/LB_TelegraphAbilityTask.h"
#include "Characters/LB_BaseCharacter.h"
#include "GameplayTags/LBTags.h"
#include "Utils/LB_BlueprintLibrary.h"

ULB_BossChargeAbility::ULB_BossChargeAbility()
{
	// 데미지 계산은 서버만 수행한다. 클라이언트가 직접 HP를 바꾸면 멀티플레이에서 치트와 불일치가 생긴다.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	
	AttackConfig.DamageEffect = DamageEffect;
	AttackConfig.Damage = Damage;
	AttackConfig.HitBoxElevationOffset = HitBoxElevationOffset;
	AttackConfig.HitBoxForwardOffset = HitBoxForwardOffset;
	AttackConfig.HitBoxRadius = HitBoxRadius;
	AttackConfig.bDrawHitDebug = bDrawHitDebug;
	AttackConfig.KnockbackForce = KnockbackForce;
	AttackConfig.KnockbackForceV = KnockbackForceV;
}

void ULB_BossChargeAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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

	if (!DamageEffect)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LB BossAttack] DamageEffect is not set. GA=%s"), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	//전조 증상 여부에 따른 Task 생성 추가
	if (bIsTelegraph)
	{
		ULB_TelegraphAbilityTask* TelegraphAbilityTask = ULB_TelegraphAbilityTask::PlayTelegraph(
			this,
			TelegraphMontage,
			LBTags::LBAbilities::Enemy::Telegraph,
			LBTags::LBCues::Enemy::TelegraphCue);
		
		TelegraphAbilityTask->OnTaskCompleted.AddDynamic(this,&ULB_BossChargeAbility::OnAbilityActivated);
		TelegraphAbilityTask->OnTaskCancelled.AddDynamic(this,&ULB_BossChargeAbility::OnAbilityCancelled);
		TelegraphAbilityTask->ReadyForActivation();
	}
	else
	{
		HandleActivateAbility(Handle,ActorInfo,ActivationInfo,TriggerEventData);
	}
}

void ULB_BossChargeAbility::OnAbilityActivated()
{
	HandleActivateAbility(
	GetCurrentAbilitySpecHandle(),
	GetCurrentActorInfo(),
	GetCurrentActivationInfo(),
	nullptr);
}

void ULB_BossChargeAbility::HandleActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	PlayMontage(AvatarActor);


	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void ULB_BossChargeAbility::OnAbilityCancelled()
{
	//CancellAbility는 다른 객체에 의해서 종료시킬 때 사용한다.
	EndAbility(GetCurrentAbilitySpecHandle()
		,GetCurrentActorInfo()
		,GetCurrentActivationInfo()
		,true
		,true);
}

void ULB_BossChargeAbility::PlayMontage(AActor* AvatarActor)
{
	UAnimMontage* SelectedMontage = PrimaryMontage;
	if (!IsValid(SelectedMontage))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LB BossAttack] No primary montage is set. Avatar=%s"), *GetNameSafe(AvatarActor));
		return;
	}

	if (ALB_BaseCharacter* BaseCharacter = Cast<ALB_BaseCharacter>(AvatarActor))
	{
		BaseCharacter->MulticastPlayCosmeticMontage(SelectedMontage, MontagePlayRate);
		UE_LOG(LogTemp, Log, TEXT("[LB BossAttack] Montage=%s Avatar=%s"), *GetNameSafe(SelectedMontage), *GetNameSafe(AvatarActor));
		return;
	}
	
}
