// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/Boss/LB_BossCharacter.h"

#include "AIController.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/LB_AbilitySystemComponent.h"
#include "AbilitySystem/LB_AttributeSet.h"
#include "Components/CapsuleComponent.h"
#include "Controller/Component/LB_AttackPatternComponent.h"
#include "Controller/Component/LB_ThreatComponent.h"
#include "GameplayTags/LBTags.h"
#include "Net/UnrealNetwork.h"

ALB_BossCharacter::ALB_BossCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		// 충돌 판정은 그대로 쓰고, 플레이 화면에서는 캡슐 디버그 선만 숨긴다.
		Capsule->SetHiddenInGame(true);
		Capsule->SetVisibility(false);
	}

	AbilitySystemComponent = CreateDefaultSubobject<ULB_AbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	Attributeset = CreateDefaultSubobject<ULB_AttributeSet>(TEXT("AttributeSet"));
	
	ThreatComponent = CreateDefaultSubobject<ULB_ThreatComponent>(TEXT("ThreatComponent"));
	AttackPatternComponent = CreateDefaultSubobject<ULB_AttackPatternComponent>(TEXT("AttackPatternComponent"));

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

//페이즈가 교체되었을 경우, 새로운 페이즈를 브로드캐스팅한다.
void ALB_BossCharacter::HandlePaseChanged(const FOnAttributeChangeData& AttributeChangeData)
{
	const int32 NewPhaseIndex = CalculatePhase(AttributeChangeData);
	if (NewPhaseIndex == INDEX_NONE || CurrentPhaseIndex >= NewPhaseIndex)
	{
		return;
	}
	
	//본래의 페이즈 태그는 지워준다.
	if (CurrentPhaseTagHandle.IsValid())
	{
		GetAbilitySystemComponent()->RemoveActiveGameplayEffect(CurrentPhaseTagHandle);
	}
	
	CurrentPhaseIndex = NewPhaseIndex;

	UE_LOG(LogTemp, Warning, TEXT("[LB Boss] Phase %d activated"), CurrentPhaseIndex + 1);
	//다음 페이즈가 유효하다면
	if (PhaseInfos.IsValidIndex(CurrentPhaseIndex))
	{
		//Phase GE에다가 태그를 붙여놓는다.
		FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(PhaseTagGrantEffectClass,1.f,GetAbilitySystemComponent()->MakeEffectContext());
		Spec.Data->DynamicGrantedTags.AddTag(PhaseInfos[CurrentPhaseIndex].PhaseTag);
		CurrentPhaseTagHandle = GetAbilitySystemComponent()->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		
		//페이즈 변화를 델리게이트 한다. 
		PhaseChange.Broadcast(PhaseInfos[CurrentPhaseIndex].PhaseTag);
	}
}

//페이즈가 현재 어느 단계인지 계산한다.
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

//능력을 적용시켜준다.
void ALB_BossCharacter::ApplyPhaseAbilities(int32 PhaseIndex)
{
	//서버가 아니거나 ASC가 정상적이 아니라면 종료
	if (!HasAuthority() || !IsValid(GetAbilitySystemComponent())) return;
	//인덱스가 값을 벗어나면 종료
	if (!PhaseInfos.IsValidIndex(PhaseIndex)) return;
	
	//SkillAbility를 적용시킨다.
	for (TSubclassOf<UGameplayAbility> SkillAbility : PhaseInfos[PhaseIndex].PhaseSkill)
	{
		if (!SkillAbility) continue;
		FGameplayAbilitySpec Spec(SkillAbility,1,INDEX_NONE,this);
		CurrentPhaseAbilityHandles.Add(GetAbilitySystemComponent()->GiveAbility(Spec));
	}
}
