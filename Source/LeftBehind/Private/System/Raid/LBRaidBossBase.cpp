//LBRaidBossBase.cpp
//테스트 보스 추가

#include "System/Raid/LBRaidBossBase.h"

#include "AbilitySystem/LB_AbilitySystemComponent.h"
#include "AbilitySystem/LB_AttributeSet.h"
#include "Components/CapsuleComponent.h"
#include "Net/UnrealNetwork.h"

ALBRaidBossBase::ALBRaidBossBase()
{
	// 보스 HP/상태와 이동을 클라이언트에 동기화한다.
	bReplicates = true;
	SetReplicateMovement(true);

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		// 충돌은 유지하되, 게임 화면에서는 캡슐 와이어가 보이지 않게 한다.
		Capsule->SetHiddenInGame(true);
		Capsule->SetVisibility(false);
	}

	AbilitySystemComponent = CreateDefaultSubobject<ULB_AbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	// 보스는 특정 플레이어 소유가 아닌 공용 전투 대상이므로 모든 클라이언트가 GE/Attribute 변화를 안정적으로 받게 한다.
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Full);

	AttributeSet = CreateDefaultSubobject<ULB_AttributeSet>(TEXT("AttributeSet"));
}

void ALBRaidBossBase::BeginPlay()
{
	Super::BeginPlay();

	if (!IsValid(AbilitySystemComponent))
	{
		return;
	}

	// 보스는 자기 자신이 능력의 주인이자 실제 몸이다.
	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	if (HasAuthority())
	{
		BindGASAttributeDelegates();
	}
}

void ALBRaidBossBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALBRaidBossBase, CurrentHP);
	DOREPLIFETIME(ALBRaidBossBase, MaxHP);
	DOREPLIFETIME(ALBRaidBossBase, DEF);
	DOREPLIFETIME(ALBRaidBossBase, bIsDead);
}

UAbilitySystemComponent* ALBRaidBossBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UAttributeSet* ALBRaidBossBase::GetAttributeSet() const
{
	return AttributeSet;
}

ULB_AttributeSet* ALBRaidBossBase::GetLBAttributeSet() const
{
	return AttributeSet;
}

float ALBRaidBossBase::GetCurrentMana() const
{
	return IsValid(AttributeSet) ? AttributeSet->GetMana() : 0.f;
}

float ALBRaidBossBase::GetMaxMana() const
{
	return IsValid(AttributeSet) ? AttributeSet->GetMaxMana() : 0.f;
}

void ALBRaidBossBase::InitializeBossStats_ServerOnly(float InMaxHP, float InMaxMana, float InDEF)
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

	if (IsValid(AbilitySystemComponent) && IsValid(AttributeSet))
	{
		// Max 값을 먼저 넣고, 그 다음 현재 HP/Mana를 최대치로 채운다.
		AbilitySystemComponent->SetNumericAttributeBase(ULB_AttributeSet::GetMaxHealthAttribute(), MaxHP);
		AbilitySystemComponent->SetNumericAttributeBase(ULB_AttributeSet::GetMaxManaAttribute(), SafeMaxMana);
		AttributeSet->FillCurrentAttributesToMax();
		CurrentHP = AttributeSet->GetHealth();
	}
	else if (IsValid(AttributeSet))
	{
		// ASC가 없는 비정상 상황에서도 최소한 수치 자체는 맞춘다.
		AttributeSet->SetMaxHealth(MaxHP);
		AttributeSet->SetMaxMana(SafeMaxMana);
		AttributeSet->FillCurrentAttributesToMax();
		CurrentHP = AttributeSet->GetHealth();
	}

	bInitializingStats = false;

	// 서버에서도 HP UI/상태 갱신 이벤트가 즉시 흐르도록 RepNotify를 직접 호출한다.
	OnRep_CurrentHP();
	ForceNetUpdate();
}

void ALBRaidBossBase::ApplyRaidDamage_ServerOnly(float DamageAmount)
{
	// 서버가 아니거나 이미 사망한 보스라면 데미지를 무시한다.
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

	if (IsValid(AbilitySystemComponent) && IsValid(AttributeSet))
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

void ALBRaidBossBase::Die_ServerOnly()
{
	// 사망 이벤트가 중복으로 방송되지 않도록 권한과 상태를 다시 확인한다.
	if (!HasAuthority() || bIsDead)
	{
		return;
	}

	bIsDead = true;
	CurrentHP = 0.f;

	if (IsValid(AttributeSet))
	{
		AttributeSet->SetHealth(0.f);
	}

	// 마지막 HP 0 상태를 먼저 알리고, 그 다음 레이드 종료 이벤트를 방송한다.
	OnRep_CurrentHP();
	OnBossDied.Broadcast();

	ForceNetUpdate();
}

void ALBRaidBossBase::OnRep_CurrentHP()
{
	// GameMode는 이 이벤트를 받아 GameState의 복제용 BossHP 값을 갱신한다.
	OnBossHPChanged.Broadcast(CurrentHP, MaxHP);
}

void ALBRaidBossBase::BindGASAttributeDelegates()
{
	if (!IsValid(AbilitySystemComponent) || !IsValid(AttributeSet))
	{
		return;
	}

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetHealthAttribute()).RemoveAll(this);
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetHealthAttribute()).AddUObject(this, &ThisClass::HandleHealthAttributeChanged);

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetMaxHealthAttribute()).RemoveAll(this);
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetMaxHealthAttribute()).AddUObject(this, &ThisClass::HandleMaxHealthAttributeChanged);
}

void ALBRaidBossBase::HandleHealthAttributeChanged(const FOnAttributeChangeData& AttributeChangeData)
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

void ALBRaidBossBase::HandleMaxHealthAttributeChanged(const FOnAttributeChangeData& AttributeChangeData)
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
