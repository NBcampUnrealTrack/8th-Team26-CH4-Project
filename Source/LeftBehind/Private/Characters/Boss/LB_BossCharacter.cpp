// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/Boss/LB_BossCharacter.h"

#include "AIController.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/LB_AbilitySystemComponent.h"
#include "AbilitySystem/LB_AttributeSet.h"
#include "GameplayTags/LBTags.h"
#include "Net/UnrealNetwork.h"

ALB_BossCharacter::ALB_BossCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	AbilitySystemComponent = CreateDefaultSubobject<ULB_AbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	Attributeset = CreateDefaultSubobject<ULB_AttributeSet>(TEXT("AttributeSet"));
}

void ALB_BossCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, bIsBeingLaunched);
}

UAbilitySystemComponent* ALB_BossCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UAttributeSet* ALB_BossCharacter::GetAttributeSet() const
{
	return Attributeset;
}

void ALB_BossCharacter::BeginPlay()
{
	Super::BeginPlay();

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!IsValid(ASC))
	{
		return;
	}

	ASC->InitAbilityActorInfo(this, this);
	OnAscInitialized.Broadcast(ASC, GetAttributeSet());

	if (!HasAuthority())
	{
		return;
	}

	GiveStartupAbilities();
	InitializeAttribute();

	ULB_AttributeSet* LBAttributeSet = Cast<ULB_AttributeSet>(GetAttributeSet());
	if (!IsValid(LBAttributeSet)) return;

	ASC->GetGameplayAttributeValueChangeDelegate(LBAttributeSet->GetHealthAttribute()).RemoveAll(this);
	ASC->GetGameplayAttributeValueChangeDelegate(LBAttributeSet->GetHealthAttribute()).AddUObject(this, &ThisClass::OnHealthChanged);
	// 보스 HP가 줄어들 때 페이즈 변경 조건도 함께 검사한다.
	ASC->GetGameplayAttributeValueChangeDelegate(LBAttributeSet->GetHealthAttribute()).AddUObject(this, &ThisClass::HandlePaseChanged);
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

void ALB_BossCharacter::HandleDeath()
{
	Super::HandleDeath();
}

void ALB_BossCharacter::HandlePaseChanged(const FOnAttributeChangeData& AttributeChangeData)
{
	const int32 NewPhaseIndex = CalculatePhase(AttributeChangeData);
	if (NewPhaseIndex == INDEX_NONE || CurrentPhaseIndex >= NewPhaseIndex)
	{
		return;
	}

	CurrentPhaseIndex = NewPhaseIndex;

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			5.0f,
			FColor::Red,
			FString::Printf(TEXT("%d Phase Activate"), CurrentPhaseIndex + 1)
		);
	}

	if (PhaseInfos.IsValidIndex(CurrentPhaseIndex))
	{
		PhaseChange.Broadcast(PhaseInfos[CurrentPhaseIndex].PhaseTag);
	}
}

int32 ALB_BossCharacter::CalculatePhase(const FOnAttributeChangeData& AttributeChangeData)
{
	int32 MatchedPhaseIndex = INDEX_NONE;

	// 여러 기준을 한 번에 넘었을 때 가장 뒤의 페이즈까지 바로 진입한다.
	for (int32 Index = 0; Index < PhaseInfos.Num(); ++Index)
	{
		if (PhaseInfos[Index].HealthThreshold >= AttributeChangeData.NewValue)
		{
			MatchedPhaseIndex = Index;
		}
	}

	return MatchedPhaseIndex;
}
