//LBRaidGameMode.cpp

#include "System/Raid/LBRaidGameMode.h"

#include "System/Raid/LBRaidGameState.h"
#include "System/Raid/LBRaidBossBase.h"
#include "System/Raid/LBPlayerState.h"

#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace
{
    void LBRaidDebug(UWorld* World, const FString& Message, const FColor Color = FColor::Yellow, const float Duration = 5.f)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);

        if (GEngine && World && World->GetNetMode() != NM_DedicatedServer)
        {
            GEngine->AddOnScreenDebugMessage(-1, Duration, Color, Message);
        }
    }

    FString LBRaidEndReasonToString(const ELBRaidEndReason EndReason)
    {
        const UEnum* EnumPtr = StaticEnum<ELBRaidEndReason>();
        return EnumPtr ? EnumPtr->GetNameStringByValue(static_cast<int64>(EndReason)) : TEXT("Unknown");
    }

    FString LBRaidEscapeCsvField(const FString& Field)
    {
        if (!Field.Contains(TEXT(",")) && !Field.Contains(TEXT("\"")) && !Field.Contains(TEXT("\n")) && !Field.Contains(TEXT("\r")))
        {
            return Field;
        }

        FString EscapedField = Field;
        EscapedField.ReplaceInline(TEXT("\""), TEXT("\"\""));
        return FString::Printf(TEXT("\"%s\""), *EscapedField);
    }
}

ALBRaidGameMode::ALBRaidGameMode()
{
    GameStateClass = ALBRaidGameState::StaticClass();
    PlayerStateClass = ALBPlayerState::StaticClass();
}

void ALBRaidGameMode::BeginPlay()
{
    Super::BeginPlay();

    LBRaidDebug(
        GetWorld(),
        FString::Printf(
            TEXT("[RaidGM] BeginPlay. GameMode=%s NetMode=%d"),
            *GetName(),
            static_cast<int32>(GetNetMode())
        ),
        FColor::White
    );

    if (ALBRaidGameState* RGS = GetLBRaidGameState())
    {
        RGS->TimeLimitSec = DefaultTimeLimitSec;
        RGS->SetRaidState_ServerOnly(ELBRaidState::Waiting);
    }
    else
    {
        LBRaidDebug(GetWorld(), TEXT("[RaidGM] ERROR: RaidGameState is null."), FColor::Red, 10.f);
        return;
    }

    if (bAutoStartOnBeginPlay)
    {
        StartCountdown();
    }
    else
    {
        LBRaidDebug(GetWorld(), TEXT("[RaidGM] bAutoStartOnBeginPlay is false. Countdown will not start."), FColor::Orange, 10.f);
    }
}

ALBRaidGameState* ALBRaidGameMode::GetLBRaidGameState() const
{
    return GetGameState<ALBRaidGameState>();
}

void ALBRaidGameMode::StartCountdown()
{
    if (bRaidEnded)
    {
        return;
    }

    ALBRaidGameState* RGS = GetLBRaidGameState();
    if (!RGS)
    {
        LBRaidDebug(GetWorld(), TEXT("[RaidGM] ERROR: Cannot start countdown. RaidGameState is null."), FColor::Red, 10.f);
        return;
    }

    const float Now = GetWorld()->GetTimeSeconds();

    RGS->CountdownEndServerTime = Now + CountdownSec;
    RGS->SetRaidState_ServerOnly(ELBRaidState::Countdown);

    GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
    GetWorldTimerManager().SetTimer(
        CountdownTimerHandle,
        this,
        &ALBRaidGameMode::StartBattle,
        CountdownSec,
        false
    );

    LBRaidDebug(
        GetWorld(),
        FString::Printf(TEXT("[RaidGM] Countdown started. %.1f sec"), CountdownSec),
        FColor::Green
    );
}

void ALBRaidGameMode::StartBattle()
{
    if (bRaidEnded)
    {
        return;
    }

    ALBRaidGameState* RGS = GetLBRaidGameState();
    if (!RGS)
    {
        LBRaidDebug(GetWorld(), TEXT("[RaidGM] ERROR: Cannot start battle. RaidGameState is null."), FColor::Red, 10.f);
        return;
    }

    LBRaidDebug(GetWorld(), TEXT("[RaidGM] StartBattle called."), FColor::Green);

    if (!SpawnBossFromData())
    {
        LBRaidDebug(GetWorld(), TEXT("[RaidGM] ERROR: Boss spawn failed. Battle not started."), FColor::Red, 10.f);
        return;
    }

    RGS->BattleStartServerTime = GetWorld()->GetTimeSeconds();
    RGS->SetRaidState_ServerOnly(ELBRaidState::Battle);

    GetWorldTimerManager().ClearTimer(TimeLimitTimerHandle);
    GetWorldTimerManager().SetTimer(
        TimeLimitTimerHandle,
        this,
        &ALBRaidGameMode::HandleTimeLimitReached,
        RGS->TimeLimitSec,
        false
    );

    if (bDebugAutoKillBoss)
    {
        GetWorldTimerManager().ClearTimer(DebugAutoKillTimerHandle);
        GetWorldTimerManager().SetTimer(
            DebugAutoKillTimerHandle,
            this,
            &ALBRaidGameMode::DebugKillBoss_ServerOnly,
            DebugAutoKillDelaySec,
            false
        );

        LBRaidDebug(
            GetWorld(),
            FString::Printf(TEXT("[RaidGM] DebugAutoKillBoss enabled. Boss will die in %.1f sec."), DebugAutoKillDelaySec),
            FColor::Orange,
            6.f
        );
    }

    LBRaidDebug(
        GetWorld(),
        FString::Printf(TEXT("[RaidGM] Battle started. TimeLimit=%.1f"), RGS->TimeLimitSec),
        FColor::Green
    );
}

bool ALBRaidGameMode::SpawnBossFromData()
{
    if (!BossStatsTable)
    {
        LBRaidDebug(GetWorld(), TEXT("[RaidGM] ERROR: BossStatsTable is null."), FColor::Red, 10.f);
        return false;
    }

    const FLBBossStatsRow* BossRow =
        BossStatsTable->FindRow<FLBBossStatsRow>(BossRowName, TEXT("SpawnBossFromData"));

    if (!BossRow)
    {
        LBRaidDebug(
            GetWorld(),
            FString::Printf(TEXT("[RaidGM] ERROR: Boss row not found. BossRowName=%s"), *BossRowName.ToString()),
            FColor::Red,
            10.f
        );
        return false;
    }

    UClass* LoadedBossClass = BossRow->BossClass.LoadSynchronous();
    if (!LoadedBossClass)
    {
        LBRaidDebug(
            GetWorld(),
            FString::Printf(TEXT("[RaidGM] ERROR: BossClass load failed. Row=%s"), *BossRowName.ToString()),
            FColor::Red,
            10.f
        );
        return false;
    }

    TArray<AActor*> SpawnPoints;
    UGameplayStatics::GetAllActorsWithTag(GetWorld(), BossSpawnTag, SpawnPoints);

    FTransform SpawnTransform;
    if (SpawnPoints.Num() > 0 && SpawnPoints[0])
    {
        SpawnTransform = SpawnPoints[0]->GetActorTransform();
    }
    else
    {
        SpawnTransform = FTransform(FRotator::ZeroRotator, FVector::ZeroVector);

        LBRaidDebug(
            GetWorld(),
            FString::Printf(TEXT("[RaidGM] WARNING: BossSpawn tag not found. Tag=%s. Spawn at origin."), *BossSpawnTag.ToString()),
            FColor::Orange,
            10.f
        );
    }

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    SpawnedBoss = GetWorld()->SpawnActor<ALBRaidBossBase>(LoadedBossClass, SpawnTransform, Params);
    if (!SpawnedBoss)
    {
        LBRaidDebug(GetWorld(), TEXT("[RaidGM] ERROR: SpawnActor returned null."), FColor::Red, 10.f);
        return false;
    }

    SpawnedBoss->OnBossHPChanged.AddDynamic(this, &ALBRaidGameMode::NotifyBossHPChanged);
    SpawnedBoss->OnBossDied.AddDynamic(this, &ALBRaidGameMode::NotifyBossDied);
    SpawnedBoss->InitializeBossStats_ServerOnly(BossRow->MaxHP, BossRow->DEF);

    if (ALBRaidGameState* RGS = GetLBRaidGameState())
    {
        RGS->TimeLimitSec = BossRow->TimeLimitSec;
        RGS->SetBossHP_ServerOnly(BossRow->MaxHP, BossRow->MaxHP);
    }

    LBRaidDebug(
        GetWorld(),
        FString::Printf(
            TEXT("[RaidGM] Boss spawned. Actor=%s HP=%.0f DEF=%.0f"),
            *SpawnedBoss->GetName(),
            BossRow->MaxHP,
            BossRow->DEF
        ),
        FColor::Green,
        8.f
    );

    return true;
}

void ALBRaidGameMode::NotifyBossHPChanged(float CurrentHP, float MaxHP)
{
    if (ALBRaidGameState* RGS = GetLBRaidGameState())
    {
        RGS->SetBossHP_ServerOnly(CurrentHP, MaxHP);
    }
}

void ALBRaidGameMode::NotifyBossDied()
{
    if (bRaidEnded)
    {
        return;
    }

    LBRaidDebug(GetWorld(), TEXT("[RaidGM] Boss died. Victory."), FColor::Cyan, 8.f);
    EndRaid(true, ELBRaidEndReason::BossKilled);
}

void ALBRaidGameMode::NotifyPlayerDied(AController* DeadController)
{
    if (bRaidEnded || !DeadController)
    {
        return;
    }

    ALBPlayerState* LBPS = DeadController->GetPlayerState<ALBPlayerState>();
    if (!LBPS)
    {
        return;
    }

    if (!LBPS->IsDead())
    {
        LBPS->SetDead_ServerOnly(true);
        LBPS->AddDeathCount_ServerOnly();
    }

    bool bAllDead = true;

    if (GameState)
    {
        for (APlayerState* PS : GameState->PlayerArray)
        {
            const ALBPlayerState* OtherPS = Cast<ALBPlayerState>(PS);
            if (OtherPS && !OtherPS->IsDead())
            {
                bAllDead = false;
                break;
            }
        }
    }

    if (bAllDead)
    {
        LBRaidDebug(GetWorld(), TEXT("[RaidGM] All players dead. Defeat."), FColor::Red, 8.f);
        EndRaid(false, ELBRaidEndReason::AllDead);
    }
}

void ALBRaidGameMode::HandleTimeLimitReached()
{
    if (bRaidEnded)
    {
        return;
    }

    LBRaidDebug(GetWorld(), TEXT("[RaidGM] Time limit reached. Defeat."), FColor::Red, 8.f);
    EndRaid(false, ELBRaidEndReason::TimeOut);
}

void ALBRaidGameMode::DebugKillBoss_ServerOnly()
{
    if (!HasAuthority())
    {
        return;
    }

    if (!SpawnedBoss)
    {
        LBRaidDebug(GetWorld(), TEXT("[RaidGM] DebugKillBoss failed. SpawnedBoss is null."), FColor::Red, 8.f);
        return;
    }

    LBRaidDebug(GetWorld(), TEXT("[RaidGM] DebugKillBoss executed."), FColor::Orange, 6.f);
    SpawnedBoss->ApplyRaidDamage_ServerOnly(SpawnedBoss->GetCurrentHP());
}

void ALBRaidGameMode::EndRaid(bool bVictory, ELBRaidEndReason EndReason)
{
    if (bRaidEnded)
    {
        return;
    }

    bRaidEnded = true;

    GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
    GetWorldTimerManager().ClearTimer(TimeLimitTimerHandle);
    GetWorldTimerManager().ClearTimer(DebugAutoKillTimerHandle);

    ALBRaidGameState* RGS = GetLBRaidGameState();
    if (!RGS)
    {
        return;
    }

    const float ClearTimeSec =
        RGS->BattleStartServerTime > 0.f
        ? GetWorld()->GetTimeSeconds() - RGS->BattleStartServerTime
        : 0.f;

    FLBRaidResultData Result;
    Result.bVictory = bVictory;
    Result.EndReason = EndReason;
    Result.ClearTimeSec = ClearTimeSec;
    Result.PlayerDeaths = GetTotalPlayerDeaths();
    Result.BossRemainingHPOnFail = bVictory ? 0.f : GetBossRemainingHP();
    Result.RankID = bVictory ? CalculateRank(ClearTimeSec) : NAME_None;

    RGS->SetRaidResult_ServerOnly(Result);
    RGS->SetRaidState_ServerOnly(ELBRaidState::Result);

    WriteRaidLog(Result);

    LBRaidDebug(
        GetWorld(),
        FString::Printf(
            TEXT("[RaidGM] EndRaid. Victory=%d ClearTime=%.2f Rank=%s Deaths=%d BossHPOnFail=%.0f"),
            Result.bVictory ? 1 : 0,
            Result.ClearTimeSec,
            *Result.RankID.ToString(),
            Result.PlayerDeaths,
            Result.BossRemainingHPOnFail
        ),
        bVictory ? FColor::Cyan : FColor::Red,
        10.f
    );
}

int32 ALBRaidGameMode::GetTotalPlayerDeaths() const
{
    int32 TotalDeaths = 0;

    if (!GameState)
    {
        return TotalDeaths;
    }

    for (APlayerState* PS : GameState->PlayerArray)
    {
        if (const ALBPlayerState* LBPS = Cast<ALBPlayerState>(PS))
        {
            TotalDeaths += LBPS->GetDeathCount();
        }
    }

    return TotalDeaths;
}

float ALBRaidGameMode::GetBossRemainingHP() const
{
    return SpawnedBoss ? SpawnedBoss->GetCurrentHP() : 0.f;
}

FName ALBRaidGameMode::CalculateRank(float ClearTimeSec) const
{
    if (!RankDataTable)
    {
        LBRaidDebug(GetWorld(), TEXT("[RaidGM] WARNING: RankDataTable is null. RankID will be None."), FColor::Orange, 8.f);
        return NAME_None;
    }

    TArray<FLBRankDataRow*> RankRows;
    RankDataTable->GetAllRows<FLBRankDataRow>(TEXT("CalculateRank"), RankRows);

    const FLBRankDataRow* BestMatchedRow = nullptr;
    for (const FLBRankDataRow* RankRow : RankRows)
    {
        if (!RankRow || RankRow->RankID.IsNone() || ClearTimeSec > RankRow->ClearTimeSec)
        {
            continue;
        }

        if (!BestMatchedRow || RankRow->ClearTimeSec < BestMatchedRow->ClearTimeSec)
        {
            BestMatchedRow = RankRow;
        }
    }

    return BestMatchedRow ? BestMatchedRow->RankID : NAME_None;
}

void ALBRaidGameMode::WriteRaidLog(const FLBRaidResultData& ResultData) const
{
    const FString LogDirectory = FPaths::ProjectSavedDir() / TEXT("RaidLogs");
    IFileManager::Get().MakeDirectory(*LogDirectory, true);

    const FString LogFilePath = LogDirectory / TEXT("RaidResults.csv");
    const bool bShouldWriteHeader = !FPaths::FileExists(LogFilePath);

    FString LogText;
    if (bShouldWriteHeader)
    {
        LogText += TEXT("Timestamp,Victory,EndReason,ClearTimeSec,RankID,PlayerDeaths,BossRemainingHPOnFail\n");
    }

    const FString Timestamp = FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S"));
    LogText += FString::Printf(
        TEXT("%s,%d,%s,%.2f,%s,%d,%.0f\n"),
        *LBRaidEscapeCsvField(Timestamp),
        ResultData.bVictory ? 1 : 0,
        *LBRaidEscapeCsvField(LBRaidEndReasonToString(ResultData.EndReason)),
        ResultData.ClearTimeSec,
        *LBRaidEscapeCsvField(ResultData.RankID.ToString()),
        ResultData.PlayerDeaths,
        ResultData.BossRemainingHPOnFail
    );

    if (!FFileHelper::SaveStringToFile(LogText, *LogFilePath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append))
    {
        LBRaidDebug(
            GetWorld(),
            FString::Printf(TEXT("[RaidGM] WARNING: Failed to write raid log. Path=%s"), *LogFilePath),
            FColor::Orange,
            8.f
        );
    }
}
