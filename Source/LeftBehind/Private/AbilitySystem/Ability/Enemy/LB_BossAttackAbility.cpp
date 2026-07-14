// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Ability/Enemy/LB_BossAttackAbility.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystem/Task/LB_TelegraphAbilityTask.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Characters/LB_BaseCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameplayTags/LBTags.h"
#include "UObject/ConstructorHelpers.h"
#include "Utils/LB_BlueprintLibrary.h"

ULB_BossAttackAbility::ULB_BossAttackAbility()
{
	// 데미지 계산은 서버만 수행한다. 클라이언트가 직접 HP를 바꾸면 멀티플레이에서 치트와 불일치가 생긴다.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// BP에서 태그를 빼먹어도 Primary 입력으로 이 Ability를 찾을 수 있게 기본 태그를 넣는다.
	FGameplayTagContainer DefaultAbilityTags;
	DefaultAbilityTags.AddTag(LBTags::LBAbilities::Primary);
	SetAssetTags(DefaultAbilityTags);

	
	

}

void ULB_BossAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
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
		
		TelegraphAbilityTask->OnTaskCompleted.AddDynamic(this,&ULB_BossAttackAbility::OnAbilityActivated);
		TelegraphAbilityTask->OnTaskCancelled.AddDynamic(this,&ULB_BossAttackAbility::OnAbilityCancelled);
		TelegraphAbilityTask->ReadyForActivation();
	}
	else
	{
		HandleActivateAbility(Handle,ActorInfo,ActivationInfo,TriggerEventData);
	}
	
	
	
	

}

void ULB_BossAttackAbility::OnAbilityActivated()
{
	HandleActivateAbility(
		GetCurrentAbilitySpecHandle(),
		GetCurrentActorInfo(),
		GetCurrentActivationInfo(),
		nullptr);
}

void ULB_BossAttackAbility::HandleActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	PlayPrimaryMontage(AvatarActor);

	TArray<AActor*> HitActors = ULB_BlueprintLibrary::FindDamageableActorsInHitBox(
		AvatarActor,
		HitBoxRadius,
		HitBoxForwardOffset,
		HitBoxElevationOffset,
		bDrawHitDebug
	);

	if (HitActors.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LB BossAttack] No damageable target. Avatar=%s"), *GetNameSafe(AvatarActor));
	}

	FGameplayEventData DamagePayload;
	DamagePayload.Instigator = AvatarActor;
	DamagePayload.EventTag = LBTags::LBAbilities::Primary;
	DamagePayload.EventMagnitude = Damage;

	int32 AppliedCount = 0;
	for (AActor* HitActor : HitActors)
	{
		
		if (ULB_BlueprintLibrary::ApplyDamageEffect_ServerOnly(
			AvatarActor,
			HitActor,
			DamageEffect,
			DamagePayload,
			LBTags::SetByCaller::Damage,
			Damage,
			LBTags::Events::Enemy::HitReact
			))
		{
			++AppliedCount;
		}
		
		if (ACharacter* HitCharacter = Cast<ACharacter>(HitActor))
		{
			FVector LaunchDir = HitCharacter->GetActorLocation() - AvatarActor->GetActorLocation();
			LaunchDir.Z = 0;
			LaunchDir.Normalize();
			
			FVector LaunchForce = LaunchDir * KnockbackForce + FVector(0.f,0.f,KnockbackForceV);
			HitCharacter->LaunchCharacter(LaunchForce,true,true);
		}
		
	
	}

	UE_LOG(LogTemp, Log, TEXT("[LB BossAttack] %s hit %d actor(s). Damage=%.1f"), *GetNameSafe(AvatarActor), AppliedCount, Damage);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void ULB_BossAttackAbility::OnAbilityCancelled()
{
	//CancellAbility는 다른 객체에 의해서 종료시킬 때 사용한다.
	EndAbility(GetCurrentAbilitySpecHandle()
		,GetCurrentActorInfo()
		,GetCurrentActivationInfo()
		,true
		,true);
}

UAnimMontage* ULB_BossAttackAbility::SelectNextPrimaryMontage()
{
	const bool bUseFirstMontage = NextMontageIndex % 2 == 0;
	UAnimMontage* SelectedMontage = bUseFirstMontage ? PrimaryMontageA.Get() : PrimaryMontageB.Get();

	// 한쪽 몽타주가 비어 있어도 공격이 멈추지 않도록 다른 쪽으로 대체한다.
	if (!IsValid(SelectedMontage))
	{
		SelectedMontage = bUseFirstMontage ? PrimaryMontageB.Get() : PrimaryMontageA.Get();
	}

	NextMontageIndex = (NextMontageIndex + 1) % 2;
	return SelectedMontage;
}

void ULB_BossAttackAbility::PlayPrimaryMontage(AActor* AvatarActor)
{
	UAnimMontage* SelectedMontage = SelectNextPrimaryMontage();
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

	// 혹시 BaseCharacter가 아닌 Character가 이 Ability를 써도 서버 화면에서는 최소한 재생되게 하는 예비 경로다.
	ACharacter* Character = Cast<ACharacter>(AvatarActor);
	USkeletalMeshComponent* MeshComponent = IsValid(Character) ? Character->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = IsValid(MeshComponent) ? MeshComponent->GetAnimInstance() : nullptr;
	if (IsValid(AnimInstance))
	{
		AnimInstance->Montage_Play(SelectedMontage, FMath::Max(0.01f, MontagePlayRate));
	}
}
