// LBRaidGameMode.cpp

#include "System/Raid/LBRaidGameMode.h"

#include "System/Raid/LBRaidGameState.h"
#include "System/Raid/LBRaidBossBase.h"
#include "System/Raid/LBPlayerState.h"

#include "Engine/DataTable.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

ALBRaidGameMode::ALBRaidGameMode()
{
    GameStateClass = ALBRaidGameState::StaticClass();
    PlayerStateClass = ALBPlayerState::StaticClass();
}

void ALBRaidGameMode::BeginPlay()
{
    Super::BeginPlay();

    if (ALBRaidGameState* RGS = GetLBRaidGameState())
    {
        RGS->TimeLimitSec = DefaultTimeLimitSec;
        RGS->SetRaidState_ServerOnly(ELBRaidState::Waiting);
    }

    if (bAutoStartOnBeginPlay)
    {
        StartCountdown();
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

    UE_LOG(LogTemp, Log, TEXT("[Raid] Countdown started."));
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
        return;
    }

    if (!SpawnBossFromData())
    {
        UE_LOG(LogTemp, Error, TEXT("[Raid] Boss spawn failed. Battle not started."));
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

    UE_LOG(LogTemp, Log, TEXT("[Raid] Battle started."));
}

bool ALBRaidGameMode::SpawnBossFromData()
{
    if (!BossStatsTable)
    {
        UE_LOG(LogTemp, Error, TEXT("[Raid] BossStatsTable is null."));
        return false;
    }

    const FLBBossStatsRow* BossRow =
        BossStatsTable->FindRow<FLBBossStatsRow>(BossRowName, TEXT("SpawnBossFromData"));

    if (!BossRow)
    {
        UE_LOG(LogTemp, Error, TEXT("[Raid] Boss row not found: %s"), *BossRowName.ToString());
        return false;
    }

    UClass* LoadedBossClass = BossRow->BossClass.LoadSynchronous();
    if (!LoadedBossClass)
    {
        UE_LOG(LogTemp, Error, TEXT("[Raid] BossClass load failed."));
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
        UE_LOG(LogTemp, Warning, TEXT("[Raid] BossSpawn tag not found. Spawn at world origin."));
    }

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    SpawnedBoss = GetWorld()->SpawnActor<ALBRaidBossBase>(LoadedBossClass, SpawnTransform, Params);
    if (!SpawnedBoss)
    {
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
        EndRaid(false, ELBRaidEndReason::AllDead);
    }
}

void ALBRaidGameMode::HandleTimeLimitReached()
{
    if (bRaidEnded)
    {
        return;
    }

    EndRaid(false, ELBRaidEndReason::TimeOut);
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
}

int32 ALBRaidGameMode::GetTotalPlayerDeaths() const
{
    if (!GameState)
    {
        return 0;
    }

    int32 TotalDeaths = 0;

    for (APlayerState* PS : GameState->PlayerArray)
    {
        const ALBPlayerState* LBPS = Cast<ALBPlayerState>(PS);
        if (LBPS)
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
        return NAME_None;
    }

    TArray<FLBRankDataRow*> Rows;
    RankDataTable->GetAllRows<FLBRankDataRow>(TEXT("CalculateRank"), Rows);

    FName BestRank = NAME_None;
    float BestThreshold = TNumericLimits<float>::Max();

    for (const FLBRankDataRow* Row : Rows)
    {
        if (!Row)
        {
            continue;
        }

        if (ClearTimeSec <= Row->ClearTimeSec && Row->ClearTimeSec < BestThreshold)
        {
            BestThreshold = Row->ClearTimeSec;
            BestRank = Row->RankID;
        }
    }

    return BestRank;
}

void ALBRaidGameMode::WriteRaidLog(const FLBRaidResultData& ResultData) const
{
    const FString Dir = FPaths::ProjectSavedDir() / TEXT("RaidLogs");
    IFileManager::Get().MakeDirectory(*Dir, true);

    const FString FilePath = Dir / TEXT("RaidResult.csv");

    if (!FPaths::FileExists(FilePath))
    {
        const FString Header =
            TEXT("Victory,EndReason,ClearTimeSec,RankID,PlayerDeaths,BossRemainingHPOnFail\n");

        FFileHelper::SaveStringToFile(
            Header,
            *FilePath,
            FFileHelper::EEncodingOptions::AutoDetect,
            &IFileManager::Get(),
            EFileWrite::FILEWRITE_Append
        );
    }

    const FString Line = FString::Printf(
        TEXT("%d,%d,%.2f,%s,%d,%.2f\n"),
        ResultData.bVictory ? 1 : 0,
        static_cast<int32>(ResultData.EndReason),
        ResultData.ClearTimeSec,
        *ResultData.RankID.ToString(),
        ResultData.PlayerDeaths,
        ResultData.BossRemainingHPOnFail
    );

    FFileHelper::SaveStringToFile(
        Line,
        *FilePath,
        FFileHelper::EEncodingOptions::AutoDetect,
        &IFileManager::Get(),
        EFileWrite::FILEWRITE_Append
    );
}

void ALBRaidGameMode::DebugDamageBoss(float DamageAmount)
{
    if (!HasAuthority() || !SpawnedBoss)
    {
        return;
    }

    SpawnedBoss->ApplyRaidDamage_ServerOnly(DamageAmount);
}