// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/Boss/LB_BossCharacter.h"

#include "AIController.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/LB_AbilitySystemComponent.h"
#include "AbilitySystem/LB_Attributeset.h"
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
	//HP 변화를 안정적으로 클라이언트에 공급
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Full);

	Attributeset = CreateDefaultSubobject<ULB_AttributeSet>(TEXT("Attributeset"));
	
	ThreatComponent = CreateDefaultSubobject<ULB_ThreatComponent>(TEXT("ThreatComponent"));
	AttackPatternComponent = CreateDefaultSubobject<ULB_AttackPatternComponent>(TEXT("AttackPatternComponent"));
	
	

}

void ALB_BossCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, bIsBeingLaunched);
	
	//보스 스탯 관리 동기화
	DOREPLIFETIME(ALB_BossCharacter, CurrentHP);
	DOREPLIFETIME(ALB_BossCharacter, MaxHP);
	DOREPLIFETIME(ALB_BossCharacter, DEF);
	DOREPLIFETIME(ALB_BossCharacter, bIsDead);
}

UAbilitySystemComponent* ALB_BossCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ALB_BossCharacter::InitializeBossStats_ServerOnly(float InMaxHP, float InMaxMana, float InDEF)
{
	// 보스 스탯의 원본은 서버만 변경한다.
	if (!HasAuthority())
	{
		return;
	}

	// 비정상 데이터로 0 이하 HP/마나/방어력이 들어오는 상황을 방지한다.
	MaxHP = FMath::Max(1.f, InMaxHP);
	const float SafeMaxMana = FMath::Max(1.f, InMaxMana);
	CurrentHP = MaxHP;
	DEF = FMath::Max(0.f, InDEF);
	bIsDead = false;
	bInitializingStats = true;

	if (IsValid(AbilitySystemComponent) && IsValid(Attributeset))
	{
		// Max 값을 먼저 넣고, 그 다음 현재 HP/Mana를 최대치로 채운다.
		AbilitySystemComponent->SetNumericAttributeBase(ULB_AttributeSet::GetMaxHealthAttribute(), MaxHP);
		AbilitySystemComponent->SetNumericAttributeBase(ULB_AttributeSet::GetMaxManaAttribute(), SafeMaxMana);
		Attributeset->FillCurrentAttributesToMax();
		CurrentHP = Attributeset->GetHealth();
	}
	else if (IsValid(Attributeset))
	{
		// ASC가 없는 비정상 상황에서도 최소한 수치 자체는 맞춘다.
		Attributeset->SetMaxHealth(MaxHP);
		Attributeset->SetMaxMana(SafeMaxMana);
		Attributeset->FillCurrentAttributesToMax();
		CurrentHP = Attributeset->GetHealth();
	}

	bInitializingStats = false;

	// 서버에서도 HP UI/상태 갱신 이벤트가 즉시 흐르도록 RepNotify를 직접 호출한다.
	OnRep_CurrentHP();
	ForceNetUpdate();
}

void ALB_BossCharacter::ApplyRaidDamage_ServerOnly(float DamageAmount)
{
	if (!HasAuthority() || bIsDead || bInitializingStats)
	{
		return;
	}

	// 음수 데미지로 HP가 회복되는 것을 막고, 0 데미지는 처리하지 않는다.
	const float SafeDamage = FMath::Max(0.f, DamageAmount);
	if (SafeDamage <= 0.f)
	{
		return;
	}

	if (IsValid(AbilitySystemComponent) && IsValid(Attributeset))
	{
		// 디버그 데미지도 ASC를 통해 넣어 실제 전투 데미지와 같은 GAS 경로를 타게 한다.
		AbilitySystemComponent->ApplyModToAttribute(ULB_AttributeSet::GetHealthAttribute(), EGameplayModOp::Additive, -SafeDamage);
		return;
	}

	// ASC가 없을 때만 남기는 호환 경로다.
	CurrentHP = FMath::Max(0.f, CurrentHP - SafeDamage);
	OnRep_CurrentHP();
	ForceNetUpdate();

	if (CurrentHP <= 0.f)
	{
		Die_ServerOnly();
	}
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

	ULB_AttributeSet* LBAttributeset = Cast<ULB_AttributeSet>(GetAttributeSet());
	if (!IsValid(LBAttributeset)) return;
	LBAttributeset->FillCurrentAttributesToMax();
	
	ASC->GetGameplayAttributeValueChangeDelegate(LBAttributeset->GetHealthAttribute()).RemoveAll(this);
	ASC->GetGameplayAttributeValueChangeDelegate(LBAttributeset->GetHealthAttribute()).AddUObject(this, &ThisClass::OnHealthChanged);
	// 보스 HP가 줄어들 때 페이즈 변경 조건도 함께 검사한다.
	ASC->GetGameplayAttributeValueChangeDelegate(LBAttributeset->GetHealthAttribute()).AddUObject(this, &ThisClass::HandlePaseChanged);
	
	
	//UI 업데이트를 위한 델리게이트 구독
	ASC->GetGameplayAttributeValueChangeDelegate(LBAttributeset->GetHealthAttribute())
	.AddUObject(this, &ThisClass::HandleHealthAttributeChanged);

	ASC->GetGameplayAttributeValueChangeDelegate(LBAttributeset->GetMaxHealthAttribute())
		.AddUObject(this, &ThisClass::HandleMaxHealthAttributeChanged);
}

void ALB_BossCharacter::HandleHealthAttributeChanged(const FOnAttributeChangeData& AttributeChangeData)
{
	if (!HasAuthority() || bIsDead)
	{
		return;
	}

	CurrentHP = FMath::Clamp(AttributeChangeData.NewValue, 0.f, MaxHP);
	OnRep_CurrentHP();
	ForceNetUpdate();

	if (CurrentHP <= 0.f)
	{
		Die_ServerOnly();
	}
}

void ALB_BossCharacter::HandleMaxHealthAttributeChanged(const FOnAttributeChangeData& AttributeChangeData)
{
	if (!HasAuthority() || bInitializingStats)
	{
		return;
	}

	MaxHP = FMath::Max(1.f, AttributeChangeData.NewValue);
	CurrentHP = FMath::Clamp(CurrentHP, 0.f, MaxHP);
	OnRep_CurrentHP();
	ForceNetUpdate();
}

UAttributeSet* ALB_BossCharacter::GetAttributeSet() const
{
	return Attributeset;
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
	
	if (bIsDead)
	{
		return;
	}

	Super::HandleDeath();
	Die_ServerOnly();
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
		ApplyPhaseAbilities(CurrentPhaseIndex);
		
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
		if (MaxHP* PhaseInfos[Index].HealthThreshold >= AttributeChangeData.NewValue)
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

void ALB_BossCharacter::Die_ServerOnly()
{
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

	OnRep_CurrentHP();
	OnBossDied.Broadcast();
	ForceNetUpdate();
}

void ALB_BossCharacter::OnRep_CurrentHP()
{
	// GameMode는 이 이벤트를 받아 GameState의 복제용 BossHP 값을 갱신한다.
	OnBossHPChanged.Broadcast(CurrentHP, MaxHP);
}