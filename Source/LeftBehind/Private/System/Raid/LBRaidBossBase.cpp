//LBRaidBossBase.cpp
//테스트 보스 추가

#include "System/Raid/LBRaidBossBase.h"

#include "Net/UnrealNetwork.h"

ALBRaidBossBase::ALBRaidBossBase()
{
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
	if (!HasAuthority())
	{
		return;
	}

	MaxHP = FMath::Max(1.f, InMaxHP);
	CurrentHP = MaxHP;
	DEF = FMath::Max(0.f, InDEF);
	bIsDead = false;

	OnRep_CurrentHP();
	ForceNetUpdate();
}

void ALBRaidBossBase::ApplyRaidDamage_ServerOnly(float DamageAmount)
{
	if (!HasAuthority() || bIsDead)
	{
		return;
	}

	const float SafeDamage = FMath::Max(0.f, DamageAmount);
	if (SafeDamage <= 0.f)
	{
		return;
	}

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
	if (!HasAuthority() || bIsDead)
	{
		return;
	}

	bIsDead = true;
	CurrentHP = 0.f;

	OnRep_CurrentHP();
	OnBossDied.Broadcast();

	ForceNetUpdate();
}

void ALBRaidBossBase::OnRep_CurrentHP()
{
	OnBossHPChanged.Broadcast(CurrentHP, MaxHP);
}