// Fill out your copyright notice in the Description page of Project Settings.


#include "GameObjects/LB_Projectile.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Characters/LB_PlayerCharacter.h"
#include "AbilitySystem/LB_AbilitySystemComponent.h"
#include "GameplayTags/LBTags.h"
#include "Player/LB_PlayerState.h"
#include "GameFramework/ProjectileMovementComponent.h"


// Sets default values
ALB_Projectile::ALB_Projectile()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>("ProjectileMovement");
}

void ALB_Projectile::NotifyActorBeginOverlap(AActor* OtherActor)
{
	
	Super::NotifyActorBeginOverlap(OtherActor);
	
	ALB_PlayerCharacter* PlayerCharacter = Cast<ALB_PlayerCharacter>(OtherActor);
	if (!IsValid(PlayerCharacter) || !PlayerCharacter->IsAlive()) return;
	
	if (!HealEffect)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LB Projectile] HealEffect is not set. Actor=%s"), *GetNameSafe(this));
		Destroy();
		return;
	}
	
	UAbilitySystemComponent* AbilitySystemComponent = PlayerCharacter->GetAbilitySystemComponent();
	if (!IsValid(AbilitySystemComponent) || !HasAuthority()) return;
	
	FGameplayEffectContextHandle ContextHandle = AbilitySystemComponent->MakeEffectContext();
	FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(HealEffect, 1.f, ContextHandle);
	
	
	if (!SpecHandle.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LB Projectile] Invalid GameplayEffect spec. Actor=%s"), *GetNameSafe(this));
		Destroy();
		return;
	}

	// HealEffect가 SetByCaller로 만들어져 있어야, 이 투사체의 Heal 값이 실제 회복량에 반영된다.
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, LBTags::SetByCaller::Heal, Heal);

	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	
	if (ALB_PlayerState* PS = Cast<ALB_PlayerState>(PlayerCharacter->GetPlayerState()))
	{
		PS->AddTotalHealingDone_ServerOnly(Heal);
	}
	Destroy();
}



