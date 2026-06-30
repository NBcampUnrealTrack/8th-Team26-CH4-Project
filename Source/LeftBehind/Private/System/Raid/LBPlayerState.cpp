// LBPlayerState.cpp

#include "System/Raid/LBPlayerState.h"

#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"

ALBPlayerState::ALBPlayerState()
{
	bReplicates = true;

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	NetUpdateFrequency = 100.f;
}

void ALBPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALBPlayerState, RoleID);
	DOREPLIFETIME(ALBPlayerState, DeathCount);
	DOREPLIFETIME(ALBPlayerState, bIsDead);
}

UAbilitySystemComponent* ALBPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ALBPlayerState::ResetRaidStats_ServerOnly()
{
	if (!HasAuthority())
	{
		return;
	}

	DeathCount = 0;
	bIsDead = false;

	OnRep_DeathCount();
	OnRep_IsDead();

	ForceNetUpdate();
}

void ALBPlayerState::SetRoleID_ServerOnly(FName NewRoleID)
{
	if (!HasAuthority())
	{
		return;
	}

	RoleID = NewRoleID;
	OnRep_RoleID();

	ForceNetUpdate();
}

void ALBPlayerState::SetDead_ServerOnly(bool bNewDead)
{
	if (!HasAuthority())
	{
		return;
	}

	bIsDead = bNewDead;
	OnRep_IsDead();

	ForceNetUpdate();
}

void ALBPlayerState::AddDeathCount_ServerOnly()
{
	if (!HasAuthority())
	{
		return;
	}

	++DeathCount;
	OnRep_DeathCount();

	ForceNetUpdate();
}

void ALBPlayerState::OnRep_RoleID()
{
	OnRoleChanged.Broadcast(RoleID);
}

void ALBPlayerState::OnRep_DeathCount()
{
	OnDeathCountChanged.Broadcast(DeathCount);
}

void ALBPlayerState::OnRep_IsDead()
{
	OnDeadStateChanged.Broadcast(bIsDead);
}