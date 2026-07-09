//LBRaidGameState.cpp

#include "GameState/LB_RaidGameState.h"

#include "DrawDebugHelpers.h"
#include "Net/UnrealNetwork.h"

static FString LBRaidStateToString(ELBRaidState State)
{
    // 디버그 로그/화면 메시지에서 enum 값을 사람이 읽을 수 있는 문자열로 변환한다.
    const UEnum* EnumPtr = StaticEnum<ELBRaidState>();
    return EnumPtr ? EnumPtr->GetNameStringByValue(static_cast<int64>(State)) : TEXT("Unknown");
}

ALB_RaidGameState::ALB_RaidGameState()
{
    // GameState의 레이드 진행 정보는 모든 클라이언트가 공유해야 한다.
    bReplicates = true;
}

void ALB_RaidGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ALB_RaidGameState, RaidState);
    DOREPLIFETIME(ALB_RaidGameState, CountdownEndServerTime);
    DOREPLIFETIME(ALB_RaidGameState, BattleStartServerTime);
    DOREPLIFETIME(ALB_RaidGameState, TimeLimitSec);
    DOREPLIFETIME(ALB_RaidGameState, BossCurrentHP);
    DOREPLIFETIME(ALB_RaidGameState, BossMaxHP);
    DOREPLIFETIME(ALB_RaidGameState, RaidResult);
}

float ALB_RaidGameState::GetCountdownRemaining() const
{
    // 카운트다운 상태가 아니면 UI가 잔여 시간을 표시하지 않도록 0을 반환한다.
    if (RaidState != ELBRaidState::Countdown)
    {
        return 0.f;
    }

    // 서버 시간 기준으로 계산하므로 클라이언트 간 타이머가 크게 어긋나지 않는다.
    return FMath::Max(0.f, CountdownEndServerTime - GetServerWorldTimeSeconds());
}

float ALB_RaidGameState::GetBattleElapsed() const
{
    // 아직 전투가 시작되지 않았다면 경과 시간이 없다.
    if (BattleStartServerTime <= 0.f)
    {
        return 0.f;
    }

    // 서버 시간이 기준이라 결과 클리어 타임과 UI 경과 시간이 같은 기준을 사용한다.
    return FMath::Max(0.f, GetServerWorldTimeSeconds() - BattleStartServerTime);
}

float ALB_RaidGameState::GetBattleRemaining() const
{
    // 전투 중일 때만 남은 시간을 표시한다.
    if (RaidState != ELBRaidState::Battle)
    {
        return 0.f;
    }

    // 음수 시간이 UI에 표시되지 않도록 0으로 클램프한다.
    return FMath::Max(0.f, TimeLimitSec - GetBattleElapsed());
}

float ALB_RaidGameState::GetBossHPRatio() const
{
    // BossMaxHP 기본값은 1이지만, 혹시 모를 0 나눗셈을 한 번 더 방어한다.
    return BossMaxHP > 0.f ? BossCurrentHP / BossMaxHP : 0.f;
}

void ALB_RaidGameState::SetRaidState_ServerOnly(ELBRaidState NewState)
{
    // 레이드 상태 전환은 서버가 결정한다.
    if (!HasAuthority())
    {
        return;
    }

    RaidState = NewState;
    // 서버에서도 클라이언트와 같은 변경 이벤트/디버그 경로를 타도록 직접 호출한다.
    OnRep_RaidState();
    ForceNetUpdate();
}

void ALB_RaidGameState::SetBossHP_ServerOnly(float CurrentHP, float MaxHP)
{
    // GameMode가 BossBase 이벤트를 받아 서버에서만 복제용 HP 값을 갱신한다.
    if (!HasAuthority())
    {
        return;
    }

    // UI 계산이 안전하도록 HP는 0 이상, MaxHP는 최소 1로 보정한다.
    const float NewBossCurrentHP = FMath::Max(0.f, CurrentHP);
    const float NewBossMaxHP = FMath::Max(1.f, MaxHP);

    if (bHasBroadcastBossHP
        && FMath::IsNearlyEqual(BossCurrentHP, NewBossCurrentHP)
        && FMath::IsNearlyEqual(BossMaxHP, NewBossMaxHP))
    {
        return;
    }

    BossCurrentHP = NewBossCurrentHP;
    BossMaxHP = NewBossMaxHP;

    // 서버 화면/로그도 클라이언트와 같은 알림 흐름을 사용한다.
    OnRep_BossHP();
    ForceNetUpdate();
}

void ALB_RaidGameState::SetRaidResult_ServerOnly(const FLBRaidResultData& NewResult)
{
    // 승패와 보상 기준 결과는 서버에서 확정한다.
    if (!HasAuthority())
    {
        return;
    }

    RaidResult = NewResult;
    // 결과 화면 이벤트를 서버에서도 즉시 발생시킨다.
    OnRep_RaidResult();
    ForceNetUpdate();
}

void ALB_RaidGameState::SetRaidScoreboardData_ServerOnly(const FLBRaidScoreboardData& InScoreboardData)
{
    if (!HasAuthority())
    {
        return;
    }
    
    RaidScoreboardData = InScoreboardData;
    
    OnRep_RaidScoreboardData();
    ForceNetUpdate();
}

void ALB_RaidGameState::MulticastRaidDebugMessage_Implementation(const FString& Message, FColor Color, float Duration)
{
    (void)Color;
    (void)Duration;
    // 테스트 메시지는 화면을 가리지 않도록 Output Log에만 남긴다.
    UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);
}

void ALB_RaidGameState::MulticastRaidDebugSphere_Implementation(FVector Location, float Radius, FColor Color, float Duration)
{
    // ServerOnly Ability에서 그린 공격 판정은 원래 서버에만 보이므로 Multicast로 모든 클라이언트 월드에 다시 그린다.
    if (UWorld* World = GetWorld())
    {
        DrawDebugSphere(World, Location, Radius, 16, Color, false, Duration);
    }
}

void ALB_RaidGameState::OnRep_RaidState()
{
    // UI가 상태 전환을 감지할 수 있도록 델리게이트를 먼저 방송한다.
    OnRaidStateChanged.Broadcast(RaidState);

    const FString Message = FString::Printf(
        TEXT("[RaidGS] RaidState = %s"),
        *LBRaidStateToString(RaidState)
    );

    UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);

}

void ALB_RaidGameState::OnRep_BossHP()
{
    if (bHasBroadcastBossHP
        && FMath::IsNearlyEqual(LastBroadcastBossCurrentHP, BossCurrentHP)
        && FMath::IsNearlyEqual(LastBroadcastBossMaxHP, BossMaxHP))
    {
        return;
    }

    bHasBroadcastBossHP = true;
    LastBroadcastBossCurrentHP = BossCurrentHP;
    LastBroadcastBossMaxHP = BossMaxHP;

    // HP 바/보스 상태 UI가 이 이벤트를 구독한다.
    OnBossHPChanged.Broadcast(BossCurrentHP, BossMaxHP);

    const FString Message = FString::Printf(
        TEXT("[RaidGS] BossHP = %.0f / %.0f"),
        BossCurrentHP,
        BossMaxHP
    );

    UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);

}

void ALB_RaidGameState::OnRep_RaidResult()
{
    // 결과 화면, 보상 안내, 로그 표시 등이 이 이벤트를 통해 최종 데이터를 받는다.
    OnRaidResultChanged.Broadcast(RaidResult);

    const FString Message = FString::Printf(
        TEXT("[RaidGS] Result Victory=%d ClearTime=%.2f Rank=%s Deaths=%d BossHPOnFail=%.0f"),
        RaidResult.bVictory ? 1 : 0,
        RaidResult.ClearTimeSec,
        *RaidResult.RankID.ToString(),
        RaidResult.PlayerDeaths,
        RaidResult.BossRemainingHPOnFail
    );

    UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);

}


void ALB_RaidGameState::OnRep_RaidScoreboardData()
{
    OnRaidScoreboardChanged.Broadcast(RaidScoreboardData);
}
