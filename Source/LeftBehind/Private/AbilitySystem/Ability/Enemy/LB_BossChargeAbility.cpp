// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Ability/Enemy/LB_BossChargeAbility.h"

#include "SNegativeActionButton.h"
#include "AbilitySystem/Task/LB_TelegraphAbilityTask.h"
#include "AbilitySystem/Task/LB_TickDamageTask.h"
#include "AbilitySystem/Ability/LB_AbilityTypes.h"
#include "AbilitySystem/Ability/LB_TelegraphIndicator.h"
#include "Characters/LB_BaseCharacter.h"
#include "GameplayTags/LBTags.h"
#include "Utils/LB_BlueprintLibrary.h"

ULB_BossChargeAbility::ULB_BossChargeAbility()
{
	// 데미지 계산은 서버만 수행한다. 클라이언트가 직접 HP를 바꾸면 멀티플레이에서 치트와 불일치가 생긴다.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	

}

void ULB_BossChargeAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                            const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                            const FGameplayEventData* TriggerEventData)
{
	AttackConfig.DamageEffect = DamageEffect;
	AttackConfig.Damage = Damage;
	AttackConfig.HitBoxElevationOffset = HitBoxElevationOffset;
	AttackConfig.HitBoxForwardOffset = HitBoxForwardOffset;
	AttackConfig.HitBoxRadius = HitBoxRadius;
	AttackConfig.bDrawHitDebug = bDrawHitDebug;
	AttackConfig.KnockbackForce = KnockbackForce;
	AttackConfig.KnockbackForceV = KnockbackForceV;
	
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
		//전조 증상이 있을 경우, Indicator를 소환한다.
		IndicatorLength = ChargingDistance;
		
		if (IndicatorClass)
		{
			FActorSpawnParameters IndicatorSpawnParams;
			FVector SpawnLocation = AvatarActor->GetActorLocation();
			SpawnLocation.Z =0;
			FRotator SpawnRotation = AvatarActor->GetActorRotation();
			if (ALB_TelegraphIndicator* Indicator = GetWorld()->SpawnActor<ALB_TelegraphIndicator>(
				IndicatorClass, SpawnLocation, SpawnRotation, IndicatorSpawnParams))
			{
				Indicator->SetAsLine(IndicatorRadius, IndicatorLength);
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
			TelegraphAbilityTask->OnTaskCompleted.AddDynamic(this,&ULB_BossChargeAbility::OnAbilityActivated);
			TelegraphAbilityTask->OnTaskCancelled.AddDynamic(this,&ULB_BossChargeAbility::OnAbilityCancelled);
			TelegraphAbilityTask->ReadyForActivation();
		}
		

	}
	else
	{
		HandleActivateAbility(Handle,ActorInfo,ActivationInfo,TriggerEventData);
	}
	


}

void ULB_BossChargeAbility::OnAbilityActivated()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	PlayMontage(AvatarActor);
	StartCharge();
	
	
}

void ULB_BossChargeAbility::HandleActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void ULB_BossChargeAbility::OnChargeCompleted()
{
	HandleActivateAbility(
GetCurrentAbilitySpecHandle(),
GetCurrentActorInfo(),
GetCurrentActivationInfo(),
nullptr);
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

void ULB_BossChargeAbility::StartCharge()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	
	if (!IsValid(AvatarActor))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LB Chargnig Ability] AvavtorActor is nullptr"));
		OnAbilityCancelled();
	}
	
	float MaxChargingTime = IsValid(PrimaryMontage) ? PrimaryMontage->GetPlayLength() + 0.5f : 3.0f;
	
	FVector Destination = AvatarActor->GetActorLocation() + AvatarActor->GetActorForwardVector()* ChargingDistance;
	ULB_TickDamageTask* TickDamageTask = ULB_TickDamageTask::CreateTickDamageTask(this,Destination, ChargingSpeed,MaxChargingTime,AttackConfig);
	if (IsValid(TickDamageTask))
	{
		TickDamageTask->OnTaskCompleted.AddDynamic(this,&ULB_BossChargeAbility::OnChargeCompleted);
		TickDamageTask->OnTimeOut.AddDynamic(this,&ULB_BossChargeAbility::OnAbilityCancelled);
		TickDamageTask->ReadyForActivation();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("TickDamageTask is not Generated it is not Valid"));
		OnAbilityCancelled();
	}
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
