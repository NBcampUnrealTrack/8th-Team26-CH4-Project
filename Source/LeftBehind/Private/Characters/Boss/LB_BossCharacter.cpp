// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Boss/LB_BossCharacter.h"
#include "AbilitySystem/LB_AbilitySystemComponent.h"
#include "AbilitySystem/LB_AttributeSet.h"
#include "GameplayTags/LBTags.h"
#include "Net/UnrealNetwork.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AIController.h"


// Sets default values
ALB_BossCharacter::ALB_BossCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	
	AbilitySystemComponent = CreateDefaultSubobject<ULB_AbilitySystemComponent>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	
	Attributeset = CreateDefaultSubobject<ULB_AttributeSet>("AttributeSet");
}

void ALB_BossCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, bIsBeingLaunched);
}

void ALB_BossCharacter::StopMovementUntilLanded()
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

void ALB_BossCharacter::EnableMovementOnLanded(const FHitResult& Hit)
{
	bIsBeingLaunched = false;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, LBTags::Events::Enemy::EndAttack, FGameplayEventData());
	LandedDelegate.RemoveAll(this);
}

UAbilitySystemComponent* ALB_BossCharacter::GetAbilitySystemComponent() const
{
	return Super::GetAbilitySystemComponent();
}

UAttributeSet* ALB_BossCharacter::GetAttributeSet() const
{
	return Attributeset;
}

void ALB_BossCharacter::HandleDeath()
{
	Super::HandleDeath();
}

void ALB_BossCharacter::HandlePaseChanged(const FOnAttributeChangeData& AttributeChangeData)
{
	int32 Phase = CalculatePhase(AttributeChangeData);
	
	if (CurrentPhaseIndex >= Phase)
	{
		return;
	}
	
	CurrentPhaseIndex++;
	
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1,5.0f, FColor::Red,FString::Printf(TEXT("%d Phase Activate"), CurrentPhaseIndex));
	}
	
	if (PhaseInfos.Num() <= CurrentPhaseIndex) return;
	
	PhaseChange.Broadcast(PhaseInfos[CurrentPhaseIndex].PhaseTag);
	
	

}

//페이즈를 계산하는 함수
int32 ALB_BossCharacter::CalculatePhase(const FOnAttributeChangeData& AttributeChangeData)
{
	for (int32 i = 0; i<PhaseInfos.Num();i++)
	{
		if (PhaseInfos[i].HealthThreshold >= AttributeChangeData.NewValue)
		{
			return i+1;
		}
	}
	return 0;
}


void ALB_BossCharacter::BeginPlay()
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
	
	ULB_AttributeSet* LB_AttributeSet = Cast<ULB_AttributeSet>(GetAttributeSet());
	if (!IsValid(LB_AttributeSet)) return;
	
	GetAbilitySystemComponent()->GetGameplayAttributeValueChangeDelegate(LB_AttributeSet->GetHealthAttribute()).AddUObject(this,&ThisClass::OnHealthChanged);
	//보스의 HP 변화에 따른 페이즈 변화를 알리기 위함.
	GetAbilitySystemComponent()->GetGameplayAttributeValueChangeDelegate(LB_AttributeSet->GetHealthAttribute()).AddUObject(this,&ThisClass::HandlePaseChanged);
	
}

