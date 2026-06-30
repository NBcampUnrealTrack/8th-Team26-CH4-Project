//LBRaidGameState.cpp

#include "System/Raid/LBRaidGameState.h"

#include "Engine/Engine.h"
#include "Net/UnrealNetwork.h"

static FString LBRaidStateToString(ELBRaidState State)
{
    const UEnum* EnumPtr = StaticEnum<ELBRaidState>();
    return EnumPtr ? EnumPtr->GetNameStringByValue(static_cast<int64>(State)) : TEXT("Unknown");
}

ALBRaidGameState::ALBRaidGameState()
{
    bReplicates = true;
}

void ALBRaidGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ALBRaidGameState, RaidState);
    DOREPLIFETIME(ALBRaidGameState, CountdownEndServerTime);
    DOREPLIFETIME(ALBRaidGameState, BattleStartServerTime);
    DOREPLIFETIME(ALBRaidGameState, TimeLimitSec);
    DOREPLIFETIME(ALBRaidGameState, BossCurrentHP);
    DOREPLIFETIME(ALBRaidGameState, BossMaxHP);
    DOREPLIFETIME(ALBRaidGameState, RaidResult);
}

float ALBRaidGameState::GetCountdownRemaining() const
{
    if (RaidState != ELBRaidState::Countdown)
    {
        return 0.f;
    }

    return FMath::Max(0.f, CountdownEndServerTime - GetServerWorldTimeSeconds());
}

float ALBRaidGameState::GetBattleElapsed() const
{
    if (BattleStartServerTime <= 0.f)
    {
        return 0.f;
    }

    return FMath::Max(0.f, GetServerWorldTimeSeconds() - BattleStartServerTime);
}

float ALBRaidGameState::GetBattleRemaining() const
{
    if (RaidState != ELBRaidState::Battle)
    {
        return 0.f;
    }

    return FMath::Max(0.f, TimeLimitSec - GetBattleElapsed());
}

float ALBRaidGameState::GetBossHPRatio() const
{
    return BossMaxHP > 0.f ? BossCurrentHP / BossMaxHP : 0.f;
}

void ALBRaidGameState::SetRaidState_ServerOnly(ELBRaidState NewState)
{
    if (!HasAuthority())
    {
        return;
    }

    RaidState = NewState;
    OnRep_RaidState();
    ForceNetUpdate();
}

void ALBRaidGameState::SetBossHP_ServerOnly(float CurrentHP, float MaxHP)
{
    if (!HasAuthority())
    {
        return;
    }

    BossCurrentHP = FMath::Max(0.f, CurrentHP);
    BossMaxHP = FMath::Max(1.f, MaxHP);

    OnRep_BossHP();
    ForceNetUpdate();
}

void ALBRaidGameState::SetRaidResult_ServerOnly(const FLBRaidResultData& NewResult)
{
    if (!HasAuthority())
    {
        return;
    }

    RaidResult = NewResult;
    OnRep_RaidResult();
    ForceNetUpdate();
}

void ALBRaidGameState::OnRep_RaidState()
{
    OnRaidStateChanged.Broadcast(RaidState);

    const FString Message = FString::Printf(
        TEXT("[RaidGS] RaidState = %s"),
        *LBRaidStateToString(RaidState)
    );

    UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);

    if (GEngine && GetNetMode() != NM_DedicatedServer)
    {
        GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Green, Message);
    }
}

void ALBRaidGameState::OnRep_BossHP()
{
    OnBossHPChanged.Broadcast(BossCurrentHP, BossMaxHP);

    const FString Message = FString::Printf(
        TEXT("[RaidGS] BossHP = %.0f / %.0f"),
        BossCurrentHP,
        BossMaxHP
    );

    UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);

    if (GEngine && GetNetMode() != NM_DedicatedServer)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, Message);
    }
}

void ALBRaidGameState::OnRep_RaidResult()
{
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

    if (GEngine && GetNetMode() != NM_DedicatedServer)
    {
        GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Cyan, Message);
    }
}