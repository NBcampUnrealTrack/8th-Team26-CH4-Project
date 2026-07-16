// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/LB_EnemyCharacter.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "AbilitySystem/LB_AbilitySystemComponent.h"
#include "AbilitySystem/LB_AttributeSet.h"
#include "Controller/Component/LB_ThreatComponent.h"
#include "Controller/Enemy/LB_EnemyBaseController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTags/LBTags.h"
#include "Net/UnrealNetwork.h"

ALB_EnemyCharacter::ALB_EnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	
	AbilitySystemComponent = CreateDefaultSubobject<ULB_AbilitySystemComponent>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	
	
	Attributeset = CreateDefaultSubobject<ULB_AttributeSet>("AttributeSet");
	bIsDead = false;
	
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;
	
}

void ALB_EnemyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ThisClass, bIsBeingLaunched);
}

void ALB_EnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	if (!IsValid(GetAbilitySystemComponent()))
	{
		return;
	}
	
	GetAbilitySystemComponent()->InitAbilityActorInfo(this,this);
	OnAscInitialized.Broadcast(GetAbilitySystemComponent(), GetAttributeSet());
	if (!HasAuthority())
	{
		return;
	}
	
	GiveStartupAbilities();
	InitializeAttribute();
	
	ULB_AttributeSet* Lb_AttributeSet = Cast<ULB_AttributeSet>(GetAttributeSet());
	if (!IsValid(Lb_AttributeSet)) return;
	
	GetAbilitySystemComponent()->GetGameplayAttributeValueChangeDelegate(Lb_AttributeSet->GetHealthAttribute()).RemoveAll(this);
	GetAbilitySystemComponent()->GetGameplayAttributeValueChangeDelegate(Lb_AttributeSet->GetHealthAttribute()).AddUObject(this,&ThisClass::OnHealthChanged);

}

void ALB_EnemyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	


}

void ALB_EnemyCharacter::Die_ServerOnly()
{
	UE_LOG(LogTemp,Warning, TEXT("LB_BossCharacter: Die_ServerOnly Activate"));
	if (!HasAuthority() || bIsDead)
	{
		return;
	}

	bIsDead = true;
	CurrentHP = 0.f;

	if (ULB_AttributeSet* LBAttributeset = Cast<ULB_AttributeSet>(GetAttributeSet()))
	{
		LBAttributeset->SetHealth(0.f);
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		FGameplayTagContainer IgnoreTags; // Death 태그는 제외
		IgnoreTags.AddTag(LBTags::Events::Enemy::Death); 
		ASC->CancelAbilities(nullptr, &IgnoreTags);
	}
	
	if (ALB_EnemyBaseController* EnemyBaseController = Cast<ALB_EnemyBaseController>(GetController()))
	{
		EnemyBaseController->BrainComponent->StopLogic(TEXT("Boss Dead"));
	}
	
	
	OnRep_CurrentHP();
	if (bIsMinions)
	{
		OnMinionsDied.Broadcast(this);
	}

	ForceNetUpdate();
}

void ALB_EnemyCharacter::OnRep_CurrentHP()
{
}

UAttributeSet* ALB_EnemyCharacter::GetAttributeSet() const
{
	return Attributeset;
}

void ALB_EnemyCharacter::HandleDeath()
{
	
	Die_ServerOnly();
	Super::HandleDeath();
	
	

	
	
	
}

void ALB_EnemyCharacter::EnableMovementOnLanded(const FHitResult& Hit)
{
	bIsBeingLaunched = false;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, LBTags::Events::Enemy::EndAttack, FGameplayEventData());
	LandedDelegate.RemoveAll(this);
}

void ALB_EnemyCharacter::StopMovementUntilLanded()
{
	bIsBeingLaunched = true;
	AAIController* AIController = GetController<AAIController>();
	if (!IsValid(AIController)) return;
	AIController->StopMovement();
	if (!LandedDelegate.IsAlreadyBound(this, &ThisClass::EnableMovementOnLanded))
	{
		LandedDelegate.AddDynamic(this, &ThisClass::EnableMovementOnLanded);
	}
}


UAbilitySystemComponent* ALB_EnemyCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}


