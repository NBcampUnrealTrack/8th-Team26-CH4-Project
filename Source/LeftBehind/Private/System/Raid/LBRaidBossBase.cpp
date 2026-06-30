//LBRaidBossBase.cpp
//테스트 보스 추가

#include "System/Raid/LBRaidBossBase.h"

#include "Net/UnrealNetwork.h"

ALBRaidBossBase::ALBRaidBossBase()
{
	// 보스 HP/상태와 이동을 클라이언트에 동기화한다.
	bReplicates = true;
	SetReplicateMovement(true);
}

void ALBRaidBossBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALBRaidBossBase, CurrentHP);
	DOREPLIFETIME(ALBRaidBossBase, MaxHP);
	DOREPLIFETIME(ALBRaidBossBase, DEF);
	DOREPLIFETIME(ALBRaidBossBase, bIsDead);
}

void ALBRaidBossBase::InitializeBossStats_ServerOnly(float InMaxHP, float InDEF)
{
	// 보스 스탯의 원본은 서버만 변경한다.
	if (!HasAuthority())
	{
		return;
	}

	// 비정상 데이터로 0 이하 HP/방어력이 들어오는 상황을 방지한다.
	MaxHP = FMath::Max(1.f, InMaxHP);
	CurrentHP = MaxHP;
	DEF = FMath::Max(0.f, InDEF);
	bIsDead = false;

	// 서버에서도 HP UI/상태 갱신 이벤트가 즉시 흐르도록 RepNotify를 직접 호출한다.
	OnRep_CurrentHP();
	ForceNetUpdate();
}

void ALBRaidBossBase::ApplyRaidDamage_ServerOnly(float DamageAmount)
{
	// 서버가 아니거나 이미 사망한 보스라면 데미지를 무시한다.
	if (!HasAuthority() || bIsDead)
	{
		return;
	}

	// 음수 데미지로 HP가 회복되는 것을 막고, 0 데미지는 처리하지 않는다.
	const float SafeDamage = FMath::Max(0.f, DamageAmount);
	if (SafeDamage <= 0.f)
	{
		return;
	}

	// HP는 항상 0 이상으로 유지한다.
	CurrentHP = FMath::Max(0.f, CurrentHP - SafeDamage);

	// 데미지 직후 HP 변경을 GameMode/GameState/UI에 알린다.
	OnRep_CurrentHP();
	ForceNetUpdate();

	// HP가 0이 되면 한 번만 사망 처리로 진입한다.
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
