// LBPlayerState.cpp

#include "Player/LB_PlayerState.h"

#include "AbilitySystem/LB_AbilitySystemComponent.h"
#include "AbilitySystem/LB_AttributeSet.h"
#include "Net/UnrealNetwork.h"

ALB_PlayerState::ALB_PlayerState()
{
	// PlayerState는 모든 클라이언트가 공유해야 하는 플레이어별 레이드 상태를 복제한다.
	bReplicates = true;

	// GAS ASC를 PlayerState에 두면 Pawn이 바뀌어도 능력/효과 상태를 유지하기 쉽다.
	AbilitySystemComponent = CreateDefaultSubobject<ULB_AbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// AttributeSet도 PlayerState가 소유해야 리스폰 후에도 서버/클라이언트가 같은 수치를 본다.
	AttributeSet = CreateDefaultSubobject<ULB_AttributeSet>(TEXT("AttributeSet"));

	// 역할/사망 상태처럼 UI에 바로 반영되어야 하는 값의 복제 빈도를 높인다.
	SetNetUpdateFrequency(100.f);
}

void ALB_PlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALB_PlayerState, RoleID);
	DOREPLIFETIME(ALB_PlayerState, DeathCount);
	DOREPLIFETIME(ALB_PlayerState, bIsDead);
}

UAbilitySystemComponent* ALB_PlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

ULB_AbilitySystemComponent* ALB_PlayerState::GetLBAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

ULB_AttributeSet* ALB_PlayerState::GetLBAttributeSet() const
{
	return AttributeSet;
}

void ALB_PlayerState::ResetRaidStats_ServerOnly()
{
	// 레이드 통계는 서버가 원본이다. 클라이언트 호출은 무시한다.
	if (!HasAuthority())
	{
		return;
	}

	DeathCount = 0;
	bIsDead = false;

	// 서버 자신에게는 RepNotify가 자동 호출되지 않으므로 동일한 알림 경로를 직접 실행한다.
	OnRep_DeathCount();
	OnRep_IsDead();

	// 변경된 값을 다음 네트워크 업데이트까지 기다리지 않고 빠르게 전송한다.
	ForceNetUpdate();
}

void ALB_PlayerState::SetRoleID_ServerOnly(FName NewRoleID)
{
	// 역할 배정은 서버에서만 확정한다.
	if (!HasAuthority())
	{
		return;
	}

	RoleID = NewRoleID;
	// 서버에서도 UI/블루프린트 델리게이트가 즉시 실행되도록 RepNotify 함수를 재사용한다.
	OnRep_RoleID();

	ForceNetUpdate();
}

void ALB_PlayerState::SetDead_ServerOnly(bool bNewDead)
{
	// 사망 상태는 모든 클라이언트가 같은 값을 보도록 서버에서만 변경한다.
	if (!HasAuthority())
	{
		return;
	}

	bIsDead = bNewDead;
	// 로컬 서버와 클라이언트 모두 같은 델리게이트 흐름을 타게 한다.
	OnRep_IsDead();

	ForceNetUpdate();
}

void ALB_PlayerState::AddDeathCount_ServerOnly()
{
	// 사망 횟수 중복 집계를 막기 위해 서버 권한에서만 증가시킨다.
	if (!HasAuthority())
	{
		return;
	}

	++DeathCount;
	// 결과 집계/화면 표시용 이벤트를 즉시 발생시킨다.
	OnRep_DeathCount();

	ForceNetUpdate();
}


FText ALB_PlayerState::GetPlayerNameText() const
{
	return FText::FromString(GetPlayerName());
}

void ALB_PlayerState::ServerRPCSetPlayerName_Implementation(const FString& InName)
{
	SetPlayerName(InName);
}

void ALB_PlayerState::OnRep_RoleID()
{
	// RoleID를 직접 읽지 않는 UI도 이벤트만 구독하면 변경을 알 수 있다.
	OnRoleChanged.Broadcast(RoleID);
}

void ALB_PlayerState::OnRep_DeathCount()
{
	// 복제 완료 시점과 서버 직접 변경 시점 모두 같은 이벤트를 사용한다.
	OnDeathCountChanged.Broadcast(DeathCount);
}

void ALB_PlayerState::OnRep_IsDead()
{
	// 사망/부활 UI, 입력 잠금, 관전 전환 같은 외부 로직이 반응하는 진입점이다.
	OnDeadStateChanged.Broadcast(bIsDead);
}