// Fill out your copyright notice in the Description page of Project Settings.


#include "GameObjects/LB_Projectile.h"
#include "Characters/LB_PlayerCharacter.h"
#include "AbilitySystem/LB_AbilitySystemComponent.h"
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
	if (!IsValid(PlayerCharacter) && !PlayerCharacter->IsAlive()) return;
	
	UAbilitySystemComponent* AbilitySystemComponent = PlayerCharacter->GetAbilitySystemComponent();
	if (!IsValid(AbilitySystemComponent) || !HasAuthority()) return;
	
	FGameplayEffectContextHandle ContextHandle = AbilitySystemComponent->MakeEffectContext();
	FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(DamageEffect, 1.f, ContextHandle);
	
	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	Destroy();
}



