//LBRaidGameMode.cpp

#include "GameMode/LB_RaidGameMode.h"

#include "GameState/LB_RaidGameState.h"
#include "System/Raid/LBRaidBossBase.h"
#include "Player/LB_PlayerState.h"

#include "Components/CapsuleComponent.h"
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
        // 서버 로그와 클라이언트 화면 디버그 메시지를 한 곳에서 같이 처리한다.
        UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);

        if (GEngine && World && World->GetNetMode() != NM_DedicatedServer)
        {
            GEngine->AddOnScreenDebugMessage(-1, Duration, Color, Message);
        }
    }

    FString LBRaidEndReasonToString(const ELBRaidEndReason EndReason)
    {
        // CSV 로그에는 enum 숫자 대신 사람이 읽을 수 있는 이름을 기록한다.
        const UEnum* EnumPtr = StaticEnum<ELBRaidEndReason>();
        return EnumPtr ? EnumPtr->GetNameStringByValue(static_cast<int64>(EndReason)) : TEXT("Unknown");
    }

    FString LBRaidEscapeCsvField(const FString& Field)
    {
        // 쉼표/따옴표/줄바꿈이 있는 값은 CSV 규칙에 맞게 따옴표로 감싸야 한다.
        if (!Field.Contains(TEXT(",")) && !Field.Contains(TEXT("\"")) && !Field.Contains(TEXT("\n")) && !Field.Contains(TEXT("\r")))
        {
            return Field;
        }

        FString EscapedField = Field;
        EscapedField.ReplaceInline(TEXT("\""), TEXT("\"\""));
        return FString::Printf(TEXT("\"%s\""), *EscapedField);
    }
}

ALB_RaidGameMode::ALB_RaidGameMode()
{
    // 레이드 모드에서는 전용 GameState/PlayerState를 사용해 상태 복제와 사망 집계를 처리한다.
    GameStateClass = ALB_RaidGameState::StaticClass();
    PlayerStateClass = ALB_PlayerState::StaticClass();
}

void ALB_RaidGameMode::BeginPlay()
{
    Super::BeginPlay();

    // 맵에서 GameMode가 정상 적용되었는지 확인하기 위한 시작 로그.
    LBRaidDebug(
        GetWorld(),
        FString::Printf(
            TEXT("[RaidGM] BeginPlay. GameMode=%s NetMode=%d"),
            *GetName(),
            static_cast<int32>(GetNetMode())
        ),
        FColor::White
    );

    if (ALB_RaidGameState* RGS = GetLBRaidGameState())
    {
        // 보스 데이터가 로드되기 전까지는 기본 제한 시간을 먼저 넣어 둔다.
        RGS->TimeLimitSec = DefaultTimeLimitSec;
        // 레이드 시작 상태를 Waiting으로 명시해 클라이언트 UI의 초기 상태를 맞춘다.
        RGS->SetRaidState_ServerOnly(ELBRaidState::Waiting);
    }
    else
    {
        LBRaidDebug(GetWorld(), TEXT("[RaidGM] ERROR: RaidGameState is null."), FColor::Red, 10.f);
        return;
    }

    if (bAutoStartOnBeginPlay)
    {
        // 테스트 맵처럼 자동 진행이 필요한 경우 BeginPlay 직후 카운트다운을 시작한다.
        StartCountdown();
    }
    else
    {
        LBRaidDebug(GetWorld(), TEXT("[RaidGM] bAutoStartOnBeginPlay is false. Countdown will not start."), FColor::Orange, 10.f);
    }
}

ALB_RaidGameState* ALB_RaidGameMode::GetLBRaidGameState() const
{
    // 캐스팅 로직을 한 곳에 모아 GameMode 내부 호출을 단순하게 만든다.
    return GetGameState<ALB_RaidGameState>();
}

void ALB_RaidGameMode::StartCountdown()
{
    // 이미 결과가 확정된 뒤에는 타이머를 새로 시작하지 않는다.
    if (bRaidEnded)
    {
        return;
    }

    ALB_RaidGameState* RGS = GetLBRaidGameState();
    if (!RGS)
    {
        LBRaidDebug(GetWorld(), TEXT("[RaidGM] ERROR: Cannot start countdown. RaidGameState is null."), FColor::Red, 10.f);
        return;
    }

    const float Now = GetWorld()->GetTimeSeconds();

    // 클라이언트가 같은 서버 시간 기준으로 남은 카운트다운을 계산하도록 종료 시각을 복제한다.
    RGS->CountdownEndServerTime = Now + CountdownSec;
    RGS->SetRaidState_ServerOnly(ELBRaidState::Countdown);

    // 중복 호출되더라도 기존 타이머를 지우고 하나만 예약한다.
    GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
    GetWorldTimerManager().SetTimer(
        CountdownTimerHandle,
        this,
        &ALB_RaidGameMode::StartBattle,
        CountdownSec,
        false
    );

    LBRaidDebug(
        GetWorld(),
        FString::Printf(TEXT("[RaidGM] Countdown started. %.1f sec"), CountdownSec),
        FColor::Green
    );
}

void ALB_RaidGameMode::StartBattle()
{
    // 레이드가 이미 끝났다면 카운트다운 타이머가 늦게 호출되어도 무시한다.
    if (bRaidEnded)
    {
        return;
    }

    ALB_RaidGameState* RGS = GetLBRaidGameState();
    if (!RGS)
    {
        LBRaidDebug(GetWorld(), TEXT("[RaidGM] ERROR: Cannot start battle. RaidGameState is null."), FColor::Red, 10.f);
        return;
    }

    LBRaidDebug(GetWorld(), TEXT("[RaidGM] StartBattle called."), FColor::Green);

    // 보스 스폰이 실패하면 Battle 상태로 넘어가지 않는다.
    if (!SpawnBossFromData())
    {
        LBRaidDebug(GetWorld(), TEXT("[RaidGM] ERROR: Boss spawn failed. Battle not started."), FColor::Red, 10.f);
        return;
    }

    // 전투 시작 시간을 복제해 UI 타이머와 클리어 타임 계산의 기준으로 사용한다.
    RGS->BattleStartServerTime = GetWorld()->GetTimeSeconds();
    RGS->SetRaidState_ServerOnly(ELBRaidState::Battle);

    // 제한 시간이 끝나면 TimeOut 패배로 종료한다.
    GetWorldTimerManager().ClearTimer(TimeLimitTimerHandle);
    GetWorldTimerManager().SetTimer(
        TimeLimitTimerHandle,
        this,
        &ALB_RaidGameMode::HandleTimeLimitReached,
        RGS->TimeLimitSec,
        false
    );

    if (bDebugAutoKillBoss)
    {
        // 테스트 편의를 위해 일정 시간 후 보스를 자동 처치할 수 있다.
        GetWorldTimerManager().ClearTimer(DebugAutoKillTimerHandle);
        GetWorldTimerManager().SetTimer(
            DebugAutoKillTimerHandle,
            this,
            &ALB_RaidGameMode::DebugKillBoss_ServerOnly,
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

bool ALB_RaidGameMode::SpawnBossFromData()
{
    // 보스 스폰에는 데이터 테이블이 필수다.
    if (!BossStatsTable)
    {
        UE_LOG(LogTemp, Error, TEXT("[RaidGM] BossStatsTable is null."));
        return false;
    }

    // 설정된 행 이름으로 보스 클래스, HP, 방어력, 제한 시간을 가져온다.
    const FLBBossStatsRow* BossRow =
        BossStatsTable->FindRow<FLBBossStatsRow>(BossRowName, TEXT("SpawnBossFromData"));

    if (!BossRow)
    {
        UE_LOG(LogTemp, Error, TEXT("[RaidGM] Boss row not found. BossRowName=%s"), *BossRowName.ToString());
        return false;
    }

    // SoftClassPtr는 실제 스폰 직전에 로드한다.
    UClass* LoadedBossClass = BossRow->BossClass.LoadSynchronous();
    if (!LoadedBossClass)
    {
        UE_LOG(LogTemp, Error, TEXT("[RaidGM] BossClass load failed. Row=%s"), *BossRowName.ToString());
        return false;
    }

    // 맵에 배치된 BossSpawnTag 액터를 찾아 첫 번째 위치를 사용한다.
    TArray<AActor*> SpawnPoints;
    UGameplayStatics::GetAllActorsWithTag(GetWorld(), BossSpawnTag, SpawnPoints);

    FVector SpawnLocation = FVector::ZeroVector;
    FRotator SpawnRotation = FRotator::ZeroRotator;

    if (SpawnPoints.Num() > 0 && SpawnPoints[0])
    {
        SpawnLocation = SpawnPoints[0]->GetActorLocation();
        SpawnRotation = SpawnPoints[0]->GetActorRotation();

        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[RaidGM] Found SpawnPoint=%s Location=%s Rotation=%s"),
            *SpawnPoints[0]->GetName(),
            *SpawnLocation.ToString(),
            *SpawnRotation.ToString()
        );
    }
    else
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[RaidGM] BossSpawn tag not found. Spawn at world origin.")
        );
    }

    // 보스 CDO에서 캡슐 크기를 읽어 바닥 기준 스폰 위치를 캐릭터 중심 위치로 보정한다.
    const ALBRaidBossBase* BossCDO = Cast<ALBRaidBossBase>(LoadedBossClass->GetDefaultObject());

    if (bBossSpawnPointIsFloorLocation && BossCDO && BossCDO->GetCapsuleComponent())
    {
        const float CapsuleHalfHeight = BossCDO->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        SpawnLocation.Z += CapsuleHalfHeight;

        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[RaidGM] Adjusted Boss Spawn Z by CapsuleHalfHeight=%.1f FinalLocation=%s"),
            CapsuleHalfHeight,
            *SpawnLocation.ToString()
        );
    }

    // 중요:
    // TargetPoint의 Scale을 그대로 쓰지 않고, 보스 Scale은 항상 1,1,1로 고정한다.
    const FTransform SpawnTransform(
        SpawnRotation,
        SpawnLocation,
        FVector::OneVector
    );

    FActorSpawnParameters Params;
    // 테스트 맵 배치 상황에서도 최대한 스폰되도록 충돌 시 위치 보정 후 강제 스폰한다.
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    // 데이터 테이블에서 로드한 보스 클래스를 실제 월드에 생성한다.
    SpawnedBoss = GetWorld()->SpawnActor<ALBRaidBossBase>(
        LoadedBossClass,
        SpawnTransform,
        Params
    );

    if (!SpawnedBoss)
    {
        UE_LOG(LogTemp, Error, TEXT("[RaidGM] SpawnActor returned null."));
        return false;
    }

    // 혹시라도 BP나 외부 설정에서 Scale이 꼬였을 경우 한 번 더 보정
    SpawnedBoss->SetActorScale3D(FVector::OneVector);
    SpawnedBoss->SetActorHiddenInGame(false);
    SpawnedBoss->SetActorEnableCollision(true);

    // 블루프린트 설정 문제로 Mesh가 숨겨져 있더라도 테스트에서 보이도록 보정한다.
    if (USkeletalMeshComponent* BossMesh = SpawnedBoss->GetMesh())
    {
        BossMesh->SetVisibility(true, true);
        BossMesh->SetHiddenInGame(false, true);
    }

    // 보스의 HP/사망 이벤트를 GameMode에 연결해 GameState 갱신과 승리 처리를 이어 준다.
    SpawnedBoss->OnBossHPChanged.AddDynamic(this, &ALB_RaidGameMode::NotifyBossHPChanged);
    SpawnedBoss->OnBossDied.AddDynamic(this, &ALB_RaidGameMode::NotifyBossDied);
    // 데이터 테이블의 수치로 보스 체력을 초기화한다.
    SpawnedBoss->InitializeBossStats_ServerOnly(BossRow->MaxHP, BossRow->DEF);

    if (ALB_RaidGameState* RGS = GetLBRaidGameState())
    {
        // 보스별 제한 시간과 초기 HP를 클라이언트 UI가 읽을 수 있도록 GameState에 복제한다.
        RGS->TimeLimitSec = BossRow->TimeLimitSec;
        RGS->SetBossHP_ServerOnly(BossRow->MaxHP, BossRow->MaxHP);
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[RaidGM] Boss spawned. Actor=%s Location=%s Scale=%s HP=%.0f DEF=%.0f"),
        *SpawnedBoss->GetName(),
        *SpawnedBoss->GetActorLocation().ToString(),
        *SpawnedBoss->GetActorScale3D().ToString(),
        BossRow->MaxHP,
        BossRow->DEF
    );

    return true;
}

void ALB_RaidGameMode::NotifyBossHPChanged(float CurrentHP, float MaxHP)
{
    // BossBase의 내부 HP 변경을 GameState의 복제용 HP 값으로 옮긴다.
    if (ALB_RaidGameState* RGS = GetLBRaidGameState())
    {
        RGS->SetBossHP_ServerOnly(CurrentHP, MaxHP);
    }
}

void ALB_RaidGameMode::NotifyBossDied()
{
    // 이미 다른 조건으로 종료되었다면 보스 사망 이벤트를 무시한다.
    if (bRaidEnded)
    {
        return;
    }

    LBRaidDebug(GetWorld(), TEXT("[RaidGM] Boss died. Victory."), FColor::Cyan, 8.f);
    EndRaid(true, ELBRaidEndReason::BossKilled);
}

void ALB_RaidGameMode::NotifyPlayerDied(AController* DeadController)
{
    // 종료 후 이벤트나 잘못된 컨트롤러 입력은 집계하지 않는다.
    if (bRaidEnded || !DeadController)
    {
        return;
    }

    // 사망한 플레이어의 레이드 전용 PlayerState를 찾아 사망 상태와 카운트를 갱신한다.
    ALB_PlayerState* LBPS = DeadController->GetPlayerState<ALB_PlayerState>();
    if (!LBPS)
    {
        return;
    }

    // 같은 사망 이벤트가 반복 호출되어도 DeathCount가 중복 증가하지 않게 한다.
    if (!LBPS->IsDead())
    {
        LBPS->SetDead_ServerOnly(true);
        LBPS->AddDeathCount_ServerOnly();
    }

    // 모든 플레이어가 사망했는지 검사해 전멸 패배를 판정한다.
    bool bAllDead = true;

    if (GameState)
    {
        for (APlayerState* PS : GameState->PlayerArray)
        {
            const ALB_PlayerState* OtherPS = Cast<ALB_PlayerState>(PS);
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

void ALB_RaidGameMode::HandleTimeLimitReached()
{
    // 보스 처치나 전멸로 이미 끝난 뒤라면 시간 초과 처리는 하지 않는다.
    if (bRaidEnded)
    {
        return;
    }

    LBRaidDebug(GetWorld(), TEXT("[RaidGM] Time limit reached. Defeat."), FColor::Red, 8.f);
    EndRaid(false, ELBRaidEndReason::TimeOut);
}

void ALB_RaidGameMode::DebugKillBoss_ServerOnly()
{
    // 디버그 처치도 실제 데미지 적용처럼 서버에서만 실행한다.
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
    // 남은 HP만큼 데미지를 넣어 일반 사망 처리 경로를 그대로 타게 한다.
    SpawnedBoss->ApplyRaidDamage_ServerOnly(SpawnedBoss->GetCurrentHP());
}

void ALB_RaidGameMode::EndRaid(bool bVictory, ELBRaidEndReason EndReason)
{
    // 보스 사망, 전멸, 시간 초과가 동시에 들어와도 결과는 한 번만 확정한다.
    if (bRaidEnded)
    {
        return;
    }

    bRaidEnded = true;

    // 종료 이후 남아 있는 타이머가 추가 상태 변경을 만들지 않도록 모두 정리한다.
    GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
    GetWorldTimerManager().ClearTimer(TimeLimitTimerHandle);
    GetWorldTimerManager().ClearTimer(DebugAutoKillTimerHandle);

    ALB_RaidGameState* RGS = GetLBRaidGameState();
    if (!RGS)
    {
        return;
    }

    // 전투 시작 시간이 없다면 비정상 종료로 보고 클리어 시간을 0으로 기록한다.
    const float ClearTimeSec =
        RGS->BattleStartServerTime > 0.f
        ? GetWorld()->GetTimeSeconds() - RGS->BattleStartServerTime
        : 0.f;

    // GameState와 CSV 로그에 남길 최종 결과 데이터를 한 번에 구성한다.
    FLBRaidResultData Result;
    Result.bVictory = bVictory;
    Result.EndReason = EndReason;
    Result.ClearTimeSec = ClearTimeSec;
    Result.PlayerDeaths = GetTotalPlayerDeaths();
    Result.BossRemainingHPOnFail = bVictory ? 0.f : GetBossRemainingHP();
    Result.RankID = bVictory ? CalculateRank(ClearTimeSec) : NAME_None;

    // 결과 데이터를 먼저 복제하고, 그 다음 Result 상태로 바꿔 UI가 완성된 데이터를 읽게 한다.
    RGS->SetRaidResult_ServerOnly(Result);
    RGS->SetRaidState_ServerOnly(ELBRaidState::Result);

    // 로컬 Saved 폴더에 결과를 누적 기록한다.
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

int32 ALB_RaidGameMode::GetTotalPlayerDeaths() const
{
    int32 TotalDeaths = 0;

    // GameState가 아직 없으면 집계할 PlayerArray도 없으므로 0을 반환한다.
    if (!GameState)
    {
        return TotalDeaths;
    }

    // 레이드 전용 PlayerState만 골라 사망 횟수를 합산한다.
    for (APlayerState* PS : GameState->PlayerArray)
    {
        if (const ALB_PlayerState* LBPS = Cast<ALB_PlayerState>(PS))
        {
            TotalDeaths += LBPS->GetDeathCount();
        }
    }

    return TotalDeaths;
}

float ALB_RaidGameMode::GetBossRemainingHP() const
{
    // 보스 스폰 실패 후 종료될 수 있으므로 null이면 0으로 처리한다.
    return SpawnedBoss ? SpawnedBoss->GetCurrentHP() : 0.f;
}

FName ALB_RaidGameMode::CalculateRank(float ClearTimeSec) const
{
    // 랭크 테이블이 없으면 클리어는 성공하더라도 랭크 없이 기록한다.
    if (!RankDataTable)
    {
        LBRaidDebug(GetWorld(), TEXT("[RaidGM] WARNING: RankDataTable is null. RankID will be None."), FColor::Orange, 8.f);
        return NAME_None;
    }

    TArray<FLBRankDataRow*> RankRows;
    RankDataTable->GetAllRows<FLBRankDataRow>(TEXT("CalculateRank"), RankRows);

    // 클리어 시간이 기준 시간 이하인 행 중 가장 빠듯한 기준을 선택한다.
    const FLBRankDataRow* BestMatchedRow = nullptr;
    for (const FLBRankDataRow* RankRow : RankRows)
    {
        // 비어 있는 행이나 기준 시간을 초과한 행은 후보에서 제외한다.
        if (!RankRow || RankRow->RankID.IsNone() || ClearTimeSec > RankRow->ClearTimeSec)
        {
            continue;
        }

        // 더 낮은 ClearTimeSec 기준이 더 높은/정확한 랭크라고 보고 갱신한다.
        if (!BestMatchedRow || RankRow->ClearTimeSec < BestMatchedRow->ClearTimeSec)
        {
            BestMatchedRow = RankRow;
        }
    }

    return BestMatchedRow ? BestMatchedRow->RankID : NAME_None;
}

void ALB_RaidGameMode::WriteRaidLog(const FLBRaidResultData& ResultData) const
{
    // 프로젝트 Saved 폴더 아래에 레이드 결과 CSV를 누적한다.
    const FString LogDirectory = FPaths::ProjectSavedDir() / TEXT("RaidLogs");
    IFileManager::Get().MakeDirectory(*LogDirectory, true);

    const FString LogFilePath = LogDirectory / TEXT("RaidResults.csv");
    // 파일이 처음 생성될 때만 헤더를 쓴다.
    const bool bShouldWriteHeader = !FPaths::FileExists(LogFilePath);

    FString LogText;
    if (bShouldWriteHeader)
    {
        LogText += TEXT("Timestamp,Victory,EndReason,ClearTimeSec,RankID,PlayerDeaths,BossRemainingHPOnFail\n");
    }

    // 한 번의 레이드 결과를 CSV 한 줄로 추가한다.
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

    // Append 모드로 저장해 이전 레이드 기록을 유지한다.
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
