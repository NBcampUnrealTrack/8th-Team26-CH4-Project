// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Ability/LB_GameplayAbility.h"

#include "NiagaraFunctionLibrary.h"
#include "GameplayTags/LBTags.h"
#include "Kismet/GameplayStatics.h"
#include "Utils/LB_BlueprintLibrary.h"

void ULB_GameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                          const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                          const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	if (bDrawDebug)
	{
		UE_LOG(LogTemp, Log, TEXT("[LB Ability] %s Activated"), *GetName());
	}
}

void ULB_GameplayAbility::SendDamageforServer(	
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	
	
	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	if (!AvatarActor->HasAuthority())
	{
		return;
	}
	
	TArray<AActor*> HitActors = ULB_BlueprintLibrary::FindDamageableActorsInHitBox(
	AvatarActor,
	AttackConfiguration.HitBoxRadius,
	AttackConfiguration.HitBoxForwardOffset,
	AttackConfiguration.HitBoxElevationOffset,
	AttackConfiguration.bDrawHitDebug
);

	if (HitActors.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LB PrimaryAttack] No damageable target. Avatar=%s"), *GetNameSafe(AvatarActor));
	}

	FGameplayEventData DamagePayload;
	DamagePayload.Instigator = AvatarActor;
	DamagePayload.EventTag = LBTags::LBAbilities::Primary;
	DamagePayload.EventMagnitude =AttackConfiguration.Damage;

	int32 AppliedCount = 0;
	for (AActor* HitActor : HitActors)
	{
		// ApplyDamageEffect_ServerOnly를 직접 부르는 대신 SendDamageEventToPlayer를 써서,
		// 치명타 여부에 따라 LBTags.Events.Player.Death / HitReact를 대상에게 자동으로 보낸다.
		if (ULB_BlueprintLibrary::SendDamageEventToPlayer(
			HitActor,
			AttackConfiguration.DamageEffect,
			DamagePayload,
			LBTags::SetByCaller::Damage,
			AttackConfiguration.Damage,
			LBTags::None))
		{
			++AppliedCount;
		}
	}
}

void ULB_GameplayAbility::PlayEffect(AActor* PlayActor, FVector PlayLocation, UNiagaraSystem* Effect)
{
	if (!PlayActor)
	{
		UE_LOG(LogTemp, Log, TEXT("[LB Ability] PlayActor inValid"));
		return;
	}
	
	if (!Effect)
	{
		UE_LOG(LogTemp, Log, TEXT("[LB Ability] Effect inValid"));
		return;
	}
	
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(),Effect,PlayLocation);
	
	
}

void ULB_GameplayAbility::PlaySound(AActor* PlayActor, FVector PlayLocation, USoundBase* SoundBase)
{
	if (!PlayActor)
	{
		UE_LOG(LogTemp, Log, TEXT("[LB Ability] PlayActor inValid"));
		return;
	}
	
	if (!SoundBase)
	{
		UE_LOG(LogTemp, Log, TEXT("[LB Ability] Sound inValid"));
		return;
	}
	
	UGameplayStatics::PlaySoundAtLocation(GetWorld(),SoundBase,PlayLocation);
	
}
