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

	// GAS 반응성은 유지하면서 100Hz PlayerState 갱신으로 생기던 불필요한 채널 검사를 줄인다.
	SetNetUpdateFrequency(30.f);
	SetMinNetUpdateFrequency(5.f);
	NetPriority = 2.f;
}

void ALB_PlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALB_PlayerState, RoleType);
	DOREPLIFETIME(ALB_PlayerState, DeathCount);
	DOREPLIFETIME(ALB_PlayerState, bIsDead);
	// 전투 중 계속 증가하는 개인 통계는 소유자에게만 보내고, 타인의 최종 수치는 글로벌 Scoreboard 한 번으로 전달한다.
	DOREPLIFETIME_CONDITION(ALB_PlayerState, TotalDamageDealt, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ALB_PlayerState, TotalHealingDone, COND_OwnerOnly);
	DOREPLIFETIME(ALB_PlayerState, bIsMVP);
	DOREPLIFETIME(ALB_PlayerState, SelectedCharacterID);
	DOREPLIFETIME(ALB_PlayerState, bCharacterReady);
	DOREPLIFETIME(ALB_PlayerState, bCodenameConfirmed);
	DOREPLIFETIME(ALB_PlayerState, bLobbyReady);
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

void ALB_PlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	if (ALB_PlayerState* TargetPlayerState = Cast<ALB_PlayerState>(PlayerState))
	{
		// PlayerName은 Super가 복사한다. 레이드 누적 통계는 의도적으로 넘기지 않는다.
		TargetPlayerState->RoleType = RoleType;
		TargetPlayerState->SelectedCharacterID = SelectedCharacterID;
		TargetPlayerState->bCharacterReady = bCharacterReady;
		TargetPlayerState->bCodenameConfirmed = bCodenameConfirmed;
	}
}

void ALB_PlayerState::OverrideWith(APlayerState* PlayerState)
{
	Super::OverrideWith(PlayerState);

	if (const ALB_PlayerState* SourcePlayerState = Cast<ALB_PlayerState>(PlayerState))
	{
		RoleType = SourcePlayerState->RoleType;
		SelectedCharacterID = SourcePlayerState->SelectedCharacterID;
		bCharacterReady = SourcePlayerState->bCharacterReady;
		bCodenameConfirmed = SourcePlayerState->bCodenameConfirmed;
	}
}

void ALB_PlayerState::ResetRaidStats_ServerOnly()
{
	// 레이드 통계는 서버가 원본이다. 클라이언트 호출은 무시한다.
	if (!HasAuthority())
	{
		return;
	}

	const bool bDeathCountChanged = DeathCount != 0;
	const bool bDeadStateChanged = bIsDead;
	const bool bStatsChanged = TotalDamageDealt != 0.f || TotalHealingDone != 0.f;
	const bool bMVPChanged = bIsMVP;
	if (!bDeathCountChanged && !bDeadStateChanged && !bStatsChanged && !bMVPChanged)
	{
		return;
	}

	DeathCount = 0;
	bIsDead = false;
	TotalDamageDealt = 0.f;
	TotalHealingDone = 0.f;
	bIsMVP = false;

	// 서버에서는 RepNotify가 자동 호출되지 않으므로 실제로 변한 이벤트만 같은 경로로 전달한다.
	if (bDeathCountChanged)
	{
		OnRep_DeathCount();
	}
	if (bDeadStateChanged)
	{
		OnRep_IsDead();
	}

	// 여러 필드를 한 번에 초기화한 뒤 한 차례만 깨워 복제 스케줄링 비용을 줄인다.
	ForceNetUpdate();
}

void ALB_PlayerState::SetRoleType_ServerOnly(ELBRoleType NewRoleType)
{
	// 역할 배정은 서버에서만 확정한다.
	if (!HasAuthority())
	{
		return;
	}

	// 같은 값을 다시 쓰면 RepNotify와 ActorChannel 강제 갱신만 중복되므로 조기에 종료한다.
	if (RoleType == NewRoleType)
	{
		return;
	}

	RoleType = NewRoleType;
	// 서버에서도 UI/블루프린트 델리게이트가 즉시 실행되도록 RepNotify 함수를 재사용한다.
	OnRep_RoleType();

	ForceNetUpdate();
}

void ALB_PlayerState::SetCharacterID_ServerOnly(ELBCharacterID NewCharacterID)
{
	if (!HasAuthority())
	{
		return;
	}

	if (SelectedCharacterID == NewCharacterID)
	{
		return;
	}
	
	SelectedCharacterID = NewCharacterID;
	
	OnRep_CharacterID();
	
	ForceNetUpdate();
}

void ALB_PlayerState::SetDead_ServerOnly(bool bNewDead)
{
	// 사망 상태는 모든 클라이언트가 같은 값을 보도록 서버에서만 변경한다.
	if (!HasAuthority())
	{
		return;
	}

	if (bIsDead == bNewDead)
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

	// 장시간 세션이나 잘못된 중복 이벤트에서도 signed overflow(정의되지 않은 동작)를 원천 차단한다.
	if (DeathCount == TNumericLimits<int32>::Max())
	{
		return;
	}

	++DeathCount;
	// 결과 집계/화면 표시용 이벤트를 즉시 발생시킨다.
	OnRep_DeathCount();

	ForceNetUpdate();
}

void ALB_PlayerState::AddTotalDamageDealt_ServerOnly(float Amount)
{
	if (!HasAuthority() || !FMath::IsFinite(Amount) || Amount <= 0.f)
	{
		return;
	}

	// float 덧셈을 double에서 계산하고 최대값으로 포화시켜 INF 전파와 결과/MVP 산식 오염을 막는다.
	const double SafeCurrentTotal = FMath::IsFinite(TotalDamageDealt) && TotalDamageDealt > 0.f
		? static_cast<double>(TotalDamageDealt)
		: 0.0;
	const double SaturatedTotal = FMath::Min(
		SafeCurrentTotal + static_cast<double>(Amount),
		static_cast<double>(TNumericLimits<float>::Max()));
	TotalDamageDealt = static_cast<float>(SaturatedTotal);
}

void ALB_PlayerState::AddTotalHealingDone_ServerOnly(float Amount)
{
	if (!HasAuthority() || !FMath::IsFinite(Amount) || Amount <= 0.f)
	{
		return;
	}

	// 피해량과 동일한 포화 정책을 사용해 통계 필드마다 수치 안정성이 달라지는 문제를 방지한다.
	const double SafeCurrentTotal = FMath::IsFinite(TotalHealingDone) && TotalHealingDone > 0.f
		? static_cast<double>(TotalHealingDone)
		: 0.0;
	const double SaturatedTotal = FMath::Min(
		SafeCurrentTotal + static_cast<double>(Amount),
		static_cast<double>(TNumericLimits<float>::Max()));
	TotalHealingDone = static_cast<float>(SaturatedTotal);
}

void ALB_PlayerState::SetMVP_ServerOnly(bool bNewMVP)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bIsMVP == bNewMVP)
	{
		return;
	}

	bIsMVP = bNewMVP;
	ForceNetUpdate();
}


FText ALB_PlayerState::GetPlayerNameText() const
{
	return FText::FromString(GetPlayerName());
}

void ALB_PlayerState::SetCodenameConfirmed_ServerOnly(bool bConfirmed)
{
	if (!HasAuthority() || bCodenameConfirmed == bConfirmed)
	{
		return;
	}

	bCodenameConfirmed = bConfirmed;
	OnRep_CodenameConfirmed();
	ForceNetUpdate();
}

void ALB_PlayerState::SetLobbyReady_ServerOnly(bool bReady)
{
	if (!HasAuthority() || bLobbyReady == bReady)
	{
		return;
	}

	bLobbyReady = bReady;
	OnRep_LobbyReady();
	ForceNetUpdate();
}

void ALB_PlayerState::SetCharacterReady_ServerOnly(bool bNewReady)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bCharacterReady == bNewReady)
	{
		return;
	}

	bCharacterReady = bNewReady;

	OnRep_CharacterReady();

	ForceNetUpdate();
}

void ALB_PlayerState::OnRep_CharacterReady()
{
	OnCharacterReadyChanged.Broadcast(bCharacterReady);
}

void ALB_PlayerState::ResetCharacterSelection_ServerOnly()
{
	if (!HasAuthority())
	{
		return;
	}

	const bool bCharacterChanged = SelectedCharacterID != ELBCharacterID::None;
	const bool bReadyChanged = bCharacterReady;

	SelectedCharacterID = ELBCharacterID::None;
	bCharacterReady = false;

	if (bCharacterChanged)
	{
		OnRep_CharacterID();
	}

	if (bReadyChanged)
	{
		OnRep_CharacterReady();
	}

	ForceNetUpdate();
}

void ALB_PlayerState::OnRep_RoleType()
{
	// RoleID를 직접 읽지 않는 UI도 이벤트만 구독하면 변경을 알 수 있다.
	OnRoleChanged.Broadcast(RoleType);
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

void ALB_PlayerState::OnRep_CharacterID()
{
	// 캐릭터 선택 UI와 파티 슬롯은 이 이벤트만 구독하면 되므로 매 프레임 PlayerState를 조회할 필요가 없다.
	OnCharacterIDChanged.Broadcast(SelectedCharacterID);
}

void ALB_PlayerState::OnRep_CodenameConfirmed()
{
	OnCodenameConfirmedChanged.Broadcast(bCodenameConfirmed);
}

void ALB_PlayerState::OnRep_LobbyReady()
{
	OnLobbyReadyChanged.Broadcast(bLobbyReady);
}
