//LBRaidGameState.cpp

#include "GameState/LB_RaidGameState.h"

#include "DrawDebugHelpers.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogLBRaidGameState, Log, All);

namespace
{
    FString LBRaidStateToString(const ELBRaidState State)
    {
        // 디버그 로그에서 enum 값을 사람이 읽을 수 있는 문자열로 변환한다.
        const UEnum* EnumPtr = StaticEnum<ELBRaidState>();
        return EnumPtr ? EnumPtr->GetNameStringByValue(static_cast<int64>(State)) : TEXT("Unknown");
    }

    float SanitizeNonNegativeFloat(const float Value)
    {
        // NaN/Infinity가 복제되면 클라이언트 UI 계산 전체를 오염시킬 수 있으므로
        // 서버의 복제 경계에서 유한한 0 이상 값으로 정규화한다.
        return FMath::IsFinite(Value) ? FMath::Max(0.f, Value) : 0.f;
    }

    FLBRaidResultData MakeSafeRaidResult(const FLBRaidResultData& Source)
    {
        FLBRaidResultData Result = Source;
        Result.ClearTimeSec = SanitizeNonNegativeFloat(Source.ClearTimeSec);
        Result.PlayerDeaths = FMath::Max(0, Source.PlayerDeaths);
        Result.BossRemainingHPOnFail = SanitizeNonNegativeFloat(Source.BossRemainingHPOnFail);
        return Result;
    }

    FLBRaidScoreboardData MakeSafeScoreboardData(const FLBRaidScoreboardData& Source)
    {
        FLBRaidScoreboardData Result = Source;
        Result.ClearTimeSec = SanitizeNonNegativeFloat(Source.ClearTimeSec);

        for (FLBPlayerFinalResult& PlayerResult : Result.PlayerResults)
        {
            PlayerResult.TotalDamageDealt = SanitizeNonNegativeFloat(PlayerResult.TotalDamageDealt);
            PlayerResult.TotalHealingDone = SanitizeNonNegativeFloat(PlayerResult.TotalHealingDone);
            PlayerResult.DeathCount = FMath::Max(0, PlayerResult.DeathCount);
        }

        return Result;
    }

    bool AreRaidResultsEqual(const FLBRaidResultData& A, const FLBRaidResultData& B)
    {
        return A.bVictory == B.bVictory
            && A.EndReason == B.EndReason
            && FMath::IsNearlyEqual(A.ClearTimeSec, B.ClearTimeSec)
            && A.RankID == B.RankID
            && A.PlayerDeaths == B.PlayerDeaths
            && FMath::IsNearlyEqual(A.BossRemainingHPOnFail, B.BossRemainingHPOnFail);
    }

    bool ArePlayerResultsEqual(const FLBPlayerFinalResult& A, const FLBPlayerFinalResult& B)
    {
        return A.PlayerName.EqualTo(B.PlayerName)
            && A.CharacterID == B.CharacterID
            && A.RoleType == B.RoleType
            && FMath::IsNearlyEqual(A.TotalDamageDealt, B.TotalDamageDealt)
            && FMath::IsNearlyEqual(A.TotalHealingDone, B.TotalHealingDone)
            && A.DeathCount == B.DeathCount
            && A.bIsMVP == B.bIsMVP;
    }

    bool AreScoreboardsEqual(const FLBRaidScoreboardData& A, const FLBRaidScoreboardData& B)
    {
        if (A.bVictory != B.bVictory
            || A.EndReason != B.EndReason
            || !FMath::IsNearlyEqual(A.ClearTimeSec, B.ClearTimeSec)
            || A.RankID != B.RankID
            || A.PlayerResults.Num() != B.PlayerResults.Num())
        {
            return false;
        }

        for (int32 Index = 0; Index < A.PlayerResults.Num(); ++Index)
        {
            if (!ArePlayerResultsEqual(A.PlayerResults[Index], B.PlayerResults[Index]))
            {
                return false;
            }
        }

        return true;
    }
}

ALB_RaidGameState::ALB_RaidGameState()
{
    // GameState의 레이드 진행 정보는 모든 클라이언트가 공유해야 한다.
    bReplicates = true;

    // HP 연타는 10Hz에서 자연스럽게 병합해 대역폭을 제한하되, 상태/결과 전환은
    // setter의 ForceNetUpdate로 즉시 전달한다. 높은 우선순위는 레이드 핵심 전역 상태의 기아를 막는다.
    SetNetUpdateFrequency(10.f);
    SetMinNetUpdateFrequency(2.f);
    NetPriority = 10.f;
}

void ALB_RaidGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // 결과/스코어보드는 late join 복구에 필요해 COND_InitialOnly로 제한하지 않는다.
    DOREPLIFETIME_CONDITION_NOTIFY(ALB_RaidGameState, RaidState, COND_None, REPNOTIFY_OnChanged);
    DOREPLIFETIME_CONDITION(ALB_RaidGameState, CountdownEndServerTime, COND_None);
    DOREPLIFETIME_CONDITION(ALB_RaidGameState, BattleStartServerTime, COND_None);
    DOREPLIFETIME_CONDITION(ALB_RaidGameState, TimeLimitSec, COND_None);
    DOREPLIFETIME_CONDITION_NOTIFY(ALB_RaidGameState, BossCurrentHP, COND_None, REPNOTIFY_OnChanged);
    DOREPLIFETIME_CONDITION_NOTIFY(ALB_RaidGameState, BossMaxHP, COND_None, REPNOTIFY_OnChanged);
    DOREPLIFETIME_CONDITION_NOTIFY(ALB_RaidGameState, RaidResult, COND_None, REPNOTIFY_OnChanged);
    DOREPLIFETIME_CONDITION_NOTIFY(ALB_RaidGameState, RaidScoreboardData, COND_None, REPNOTIFY_OnChanged);
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
    // 손상된 입력이나 순간적인 복제 순서 차이에도 ProgressBar 계약인 0~1을 보장한다.
    return BossMaxHP > 0.f && FMath::IsFinite(BossCurrentHP) && FMath::IsFinite(BossMaxHP)
        ? FMath::Clamp(BossCurrentHP / BossMaxHP, 0.f, 1.f)
        : 0.f;
}

void ALB_RaidGameState::SetRaidState_ServerOnly(ELBRaidState NewState)
{
    // 레이드 상태 전환은 서버가 결정한다.
    if (!HasAuthority())
    {
        return;
    }

    // 동일 상태를 다시 방송하면 UI 애니메이션과 즉시 네트워크 갱신이 중복되므로 조기에 종료한다.
    if (RaidState == NewState)
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

    // UI 계산이 안전하도록 비유한 값은 제거하고 CurrentHP를 유효한 최대 HP 범위로 제한한다.
    const float NewBossMaxHP = FMath::IsFinite(MaxHP) && MaxHP > 0.f ? MaxHP : 1.f;
    const float NewBossCurrentHP = FMath::IsFinite(CurrentHP)
        ? FMath::Clamp(CurrentHP, 0.f, NewBossMaxHP)
        : 0.f;

    if (FMath::IsNearlyEqual(BossCurrentHP, NewBossCurrentHP)
        && FMath::IsNearlyEqual(BossMaxHP, NewBossMaxHP))
    {
        return;
    }

    BossCurrentHP = NewBossCurrentHP;
    BossMaxHP = NewBossMaxHP;

    // 서버 UI/관찰자는 즉시 이벤트를 받지만, 일반 HP 타격마다 ForceNetUpdate는 하지 않는다.
    // 10Hz Actor 갱신에서 연속 타격을 병합하면 대규모 파티의 대역폭 burst를 줄일 수 있다.
    OnRep_BossHP();
}

void ALB_RaidGameState::SetRaidResult_ServerOnly(const FLBRaidResultData& NewResult)
{
    // 승패와 보상 기준 결과는 서버에서 확정한다.
    if (!HasAuthority())
    {
        return;
    }

    const FLBRaidResultData SafeResult = MakeSafeRaidResult(NewResult);
    if (AreRaidResultsEqual(RaidResult, SafeResult))
    {
        return;
    }

    RaidResult = SafeResult;
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
    
    const FLBRaidScoreboardData SafeScoreboardData = MakeSafeScoreboardData(InScoreboardData);
    if (AreScoreboardsEqual(RaidScoreboardData, SafeScoreboardData))
    {
        return;
    }

    RaidScoreboardData = SafeScoreboardData;
    
    OnRep_RaidScoreboardData();
    ForceNetUpdate();
}

void ALB_RaidGameState::SetRaidOutcome_ServerOnly(
    const FLBRaidResultData& NewResult,
    const FLBRaidScoreboardData& InScoreboardData)
{
    if (!HasAuthority())
    {
        return;
    }

    const FLBRaidResultData SafeResult = MakeSafeRaidResult(NewResult);
    const FLBRaidScoreboardData SafeScoreboardData = MakeSafeScoreboardData(InScoreboardData);
    bool bAnyValueChanged = false;

    // 서버 이벤트 순서를 Result -> Scoreboard -> State로 고정하면 Result UI가
    // 부분 갱신된 스냅샷을 관찰하지 않는다. 개별 setter의 세 번 강제 전송도 한 번으로 합친다.
    if (!AreRaidResultsEqual(RaidResult, SafeResult))
    {
        RaidResult = SafeResult;
        OnRep_RaidResult();
        bAnyValueChanged = true;
    }

    if (!AreScoreboardsEqual(RaidScoreboardData, SafeScoreboardData))
    {
        RaidScoreboardData = SafeScoreboardData;
        OnRep_RaidScoreboardData();
        bAnyValueChanged = true;
    }

    if (RaidState != ELBRaidState::Result)
    {
        RaidState = ELBRaidState::Result;
        OnRep_RaidState();
        bAnyValueChanged = true;
    }

    if (bAnyValueChanged)
    {
        ForceNetUpdate();
    }
}

void ALB_RaidGameState::MulticastRaidDebugMessage_Implementation(const FString& Message, FColor Color, float Duration)
{
    (void)Color;
    (void)Duration;
    // 외부 디버그 코드와의 RPC 시그니처 호환성은 유지하되 화면 메시지는 만들지 않는다.
    UE_LOG(LogLBRaidGameState, Warning, TEXT("%s"), *Message);
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

    UE_LOG(
        LogLBRaidGameState,
        Log,
        TEXT("[RaidGS] RaidState = %s"),
        *LBRaidStateToString(RaidState));
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

    // HP는 고빈도 이벤트이므로 기본 로그 노이즈와 문자열 할당을 만들지 않도록 VeryVerbose로 제한한다.
    UE_LOG(
        LogLBRaidGameState,
        VeryVerbose,
        TEXT("[RaidGS] BossHP = %.0f / %.0f"),
        BossCurrentHP,
        BossMaxHP);
}

void ALB_RaidGameState::OnRep_RaidResult()
{
    // 결과 화면, 보상 안내, 로그 표시 등이 이 이벤트를 통해 최종 데이터를 받는다.
    OnRaidResultChanged.Broadcast(RaidResult);

    UE_LOG(
        LogLBRaidGameState,
        Log,
        TEXT("[RaidGS] Result Victory=%d ClearTime=%.2f Rank=%s Deaths=%d BossHPOnFail=%.0f"),
        RaidResult.bVictory ? 1 : 0,
        RaidResult.ClearTimeSec,
        *RaidResult.RankID.ToString(),
        RaidResult.PlayerDeaths,
        RaidResult.BossRemainingHPOnFail);
}


void ALB_RaidGameState::OnRep_RaidScoreboardData()
{
    OnRaidScoreboardChanged.Broadcast(RaidScoreboardData);
}
