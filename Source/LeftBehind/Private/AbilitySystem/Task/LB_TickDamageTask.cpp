// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Task/LB_TickDamageTask.h"

#include "Characters/LB_PlayerCharacter.h"
#include "GameplayTags/LBTags.h"
#include "Utils/LB_BlueprintLibrary.h"



ULB_TickDamageTask* ULB_TickDamageTask::CreateTickDamageTask(UGameplayAbility* OwningAbility,
	 FVector TargetLocation, float ChargingSpeed, float MaxDuration,
	FLB_AttackConfig AC)
{
	ULB_TickDamageTask* TickDamageTask = NewAbilityTask<ULB_TickDamageTask>(OwningAbility);
	
	TickDamageTask->Destination = TargetLocation;
	TickDamageTask->TaskCharingSpeed = ChargingSpeed;
	TickDamageTask ->TaskMaxDuration = MaxDuration;
	
	TickDamageTask->AttackConfig.DamageEffect = AC.DamageEffect;
	TickDamageTask->AttackConfig.Damage = AC.Damage;
	TickDamageTask->AttackConfig.HitBoxElevationOffset = AC.HitBoxElevationOffset;
	TickDamageTask->AttackConfig.HitBoxForwardOffset = AC.HitBoxForwardOffset;
	TickDamageTask->AttackConfig.HitBoxRadius = AC.HitBoxRadius;
	TickDamageTask->AttackConfig.bDrawHitDebug = AC.bDrawHitDebug;
	TickDamageTask->AttackConfig.KnockbackForce = AC.KnockbackForce;
	TickDamageTask->AttackConfig.KnockbackForceV = AC.KnockbackForceV;
	
	return TickDamageTask;
}

void ULB_TickDamageTask::Activate()
{
	Super::Activate();

	StartLocation = GetAvatarActor()->GetActorLocation();
	CurrentTime = 0.f;
	
	bTickingTask = true;
}

void ULB_TickDamageTask::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);
	UE_LOG(LogTemp, Warning, TEXT("ULB_TickDamageTask Tick Activate"));
	StartLocation = GetAvatarActor()->GetActorLocation();
	FVector Direction = (Destination- StartLocation).GetSafeNormal();
	float RemainingDistance = FVector::Dist(Destination,StartLocation);
	
	FVector MoveDelta = Direction* TaskCharingSpeed *DeltaTime;
	FHitResult HitResult;
	GetAvatarActor()->SetActorLocation(StartLocation+ MoveDelta,false, &HitResult);
	
	HandleDamageableActorsInHitBox();
	
	
	if (RemainingDistance <= MarginDistance)
	{
		OnTaskCompleted.Broadcast();
		EndTask();
		return;
	}
	
	CurrentTime +=DeltaTime;
	if (CurrentTime >= TaskMaxDuration)
	{
		OnTimeOut.Broadcast();
		EndTask();
		return;
	}
}

void ULB_TickDamageTask::OnDestroy(bool bInOwnerFinished)
{
	Super::OnDestroy(bInOwnerFinished);
	bTickingTask = false;
}

void ULB_TickDamageTask::HandleDamageableActorsInHitBox()
{
	
	UE_LOG(LogTemp, Warning,
	TEXT("[LB ChargingAttack] AttackConfig. HBR : %f, HBFO : %f, HBEO : %f, bDrawHitDebug : %d"),
	AttackConfig.HitBoxRadius,
	AttackConfig.HitBoxForwardOffset,
	AttackConfig.HitBoxElevationOffset,
	AttackConfig.bDrawHitDebug);
	
		TArray<AActor*> HitActors = ULB_BlueprintLibrary::FindDamageableActorsInHitBox(
GetAvatarActor(),
AttackConfig.HitBoxRadius,
AttackConfig.HitBoxForwardOffset,
AttackConfig.HitBoxElevationOffset,
AttackConfig.bDrawHitDebug
);

	if (HitActors.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LB ChargingAttack] No damageable target. Avatar=%s"), *GetNameSafe(GetAvatarActor()));
		return;
	}

	FGameplayEventData DamagePayload;
	DamagePayload.Instigator = GetAvatarActor();
	DamagePayload.EventTag = LBTags::LBAbilities::Primary;
	DamagePayload.EventMagnitude = AttackConfig.Damage;

	int32 AppliedCount = 0;
	for (AActor* HitActor : HitActors)
	{
		
		if (ULB_BlueprintLibrary::ApplyDamageEffect_ServerOnly(
			GetAvatarActor(),
			HitActor,
			AttackConfig.DamageEffect,
			DamagePayload,
			LBTags::SetByCaller::Damage,
			AttackConfig.Damage,
			LBTags::Events::Enemy::HitReact
			))
		{
			++AppliedCount;
		}
		
		if (ACharacter* HitCharacter = Cast<ACharacter>(HitActor))
		{
			FVector LaunchDir = HitCharacter->GetActorLocation() - GetAvatarActor()->GetActorLocation();
			LaunchDir.Z = 0;
			LaunchDir.Normalize();
			
			FVector LaunchForce = LaunchDir * AttackConfig.KnockbackForce + FVector(0.f,0.f,AttackConfig.KnockbackForceV);
			HitCharacter->LaunchCharacter(LaunchForce,true,true);
		}
		
	
	}

	UE_LOG(LogTemp, Log, TEXT("[LB ChargingAttack] %s hit %d actor(s). Damage=%.1f"), *GetNameSafe(GetAvatarActor()), AppliedCount, AttackConfig.Damage);
}


