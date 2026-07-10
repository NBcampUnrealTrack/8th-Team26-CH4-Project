//LBRaidGameMode.cpp

#include "GameMode/LB_RaidGameMode.h"
#include "GameMode/LB_RaidMVPUtils.h"

#include "Characters/Boss/LB_BossCharacter.h"
#include "GameState/LB_RaidGameState.h"
#include "Player/LB_PlayerState.h"
#include "System/Raid/LBRaidDataRows.h"
#include "System/Raid/LBRaidTypes.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/DataTable.h"
#include "Engine/StreamableManager.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogLBRaidGameMode, Log, All);

namespace
{
    constexpr float LBFallbackRaidTimeLimitSec = 300.f;

    void LBRaidDebug(UWorld* World, const FString& Message, const FColor Color = FColor::Yellow, const float Duration = 5.f)
    {
        (void)World;
        (void)Duration;

        // 정상 생명주기 로그를 문자열 Multicast RPC로 보내면 접속 인원수만큼 대역폭이
        // 증가한다. 서버 전용 카테고리 로그로 남기고 외부 디버그 RPC 자체는 GameState에 보존한다.
        if (Color == FColor::Red)
        {
            UE_LOG(LogLBRaidGameMode, Error, TEXT("%s"), *Message);
        }
        else if (Color == FColor::Orange)
        {
            UE_LOG(LogLBRaidGameMode, Warning, TEXT("%s"), *Message);
        }
        else
        {
            UE_LOG(LogLBRaidGameMode, Log, TEXT("%s"), *Message);
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
        // 에디터 외 경로에서 손상된 설정이 들어와도 타이머가 즉시 종료되거나 영구 정지하지 않게 한다.
        RGS->TimeLimitSec = FMath::IsFinite(DefaultTimeLimitSec) && DefaultTimeLimitSec > 0.f
            ? DefaultTimeLimitSec
            : LBFallbackRaidTimeLimitSec;
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

void ALB_RaidGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // seamless travel/PIE 종료 중 지연 콜백이 파괴 중인 GameMode를 다시 호출하지 않도록
    // 모든 비동기 진입점을 Super::EndPlay 전에 명시적으로 닫는다.
    GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
    GetWorldTimerManager().ClearTimer(TimeLimitTimerHandle);
    CancelBossClassPreload();

    if (ALB_BossCharacter* Boss = SpawnedBoss.Get())
    {
        Boss->OnBossHPChanged.RemoveDynamic(this, &ALB_RaidGameMode::NotifyBossHPChanged);
        Boss->OnBossDied.RemoveDynamic(this, &ALB_RaidGameMode::NotifyBossDied);
    }

    SpawnedBoss.Reset();
    CachedRaidGameState.Reset();

    Super::EndPlay(EndPlayReason);
}

ALB_RaidGameState* ALB_RaidGameMode::GetLBRaidGameState()
{
    if (CachedRaidGameState.IsValid())
    {
        return CachedRaidGameState.Get();
    }

    // 캐스팅을 한 곳에서 한 번만 수행하고 weak cache로 보관해 GC/월드 수명은 침범하지 않는다.
    ALB_RaidGameState* RaidGameState = GetGameState<ALB_RaidGameState>();
    CachedRaidGameState = RaidGameState;
    return RaidGameState;
}

const FLBBossStatsRow* ALB_RaidGameMode::FindValidatedBossRow(const TCHAR* Context) const
{
    if (!BossStatsTable)
    {
        UE_LOG(LogLBRaidGameMode, Error, TEXT("[RaidGM] BossStatsTable is null. Context=%s"), Context);
        return nullptr;
    }

    const FLBBossStatsRow* BossRow = BossStatsTable->FindRow<FLBBossStatsRow>(BossRowName, Context);
    if (!BossRow)
    {
        UE_LOG(
            LogLBRaidGameMode,
            Error,
            TEXT("[RaidGM] Boss row not found. BossRowName=%s Context=%s"),
            *BossRowName.ToString(),
            Context);
        return nullptr;
    }

    // 보스 클래스와 최대 HP는 전투 성립에 필수다. 잘못된 행을 Countdown/Battle 경계 밖에서 차단한다.
    if (BossRow->BossClass.IsNull())
    {
        UE_LOG(LogLBRaidGameMode, Error, TEXT("[RaidGM] BossClass is empty. Row=%s"), *BossRowName.ToString());
        return nullptr;
    }

    if (!FMath::IsFinite(BossRow->MaxHP) || BossRow->MaxHP <= 0.f)
    {
        UE_LOG(
            LogLBRaidGameMode,
            Error,
            TEXT("[RaidGM] Boss MaxHP must be finite and positive. Row=%s MaxHP=%f"),
            *BossRowName.ToString(),
            BossRow->MaxHP);
        return nullptr;
    }

    return BossRow;
}

void ALB_RaidGameMode::RequestBossClassPreload(const FLBBossStatsRow& BossRow)
{
    CancelBossClassPreload();

    // 이미 메모리에 있는 클래스라면 별도 핸들과 비동기 요청을 만들 필요가 없다.
    if (BossRow.BossClass.Get())
    {
        return;
    }

    const FSoftObjectPath BossClassPath = BossRow.BossClass.ToSoftObjectPath();
    if (!BossClassPath.IsValid())
    {
        UE_LOG(LogLBRaidGameMode, Error, TEXT("[RaidGM] BossClass path is invalid. Row=%s"), *BossRowName.ToString());
        return;
    }

    // 카운트다운과 로드를 겹쳐 전투 시작 프레임의 package load hitch를 숨긴다.
    BossClassLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
        BossClassPath,
        FStreamableDelegate(),
        FStreamableManager::DefaultAsyncLoadPriority,
        false,
        false,
        TEXT("LB_RaidBossClass"));

    if (!BossClassLoadHandle.IsValid())
    {
        UE_LOG(
            LogLBRaidGameMode,
            Warning,
            TEXT("[RaidGM] Failed to queue BossClass preload. Sync fallback will be used. Row=%s"),
            *BossRowName.ToString());
    }
}

void ALB_RaidGameMode::CancelBossClassPreload()
{
    if (BossClassLoadHandle.IsValid())
    {
        BossClassLoadHandle->CancelHandle();
        BossClassLoadHandle.Reset();
    }
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

    // 상태 머신 계약을 서버 한 곳에서 강제해 Battle/Result 중 재호출과 중복 보스 스폰을 막는다.
    if (RGS->RaidState != ELBRaidState::Waiting && RGS->RaidState != ELBRaidState::Countdown)
    {
        UE_LOG(
            LogLBRaidGameMode,
            Warning,
            TEXT("[RaidGM] StartCountdown ignored in state %d."),
            static_cast<int32>(RGS->RaidState));
        return;
    }

    const bool bRestartingCountdown = RGS->RaidState == ELBRaidState::Countdown;
    const FLBBossStatsRow* BossRow = FindValidatedBossRow(TEXT("StartCountdown"));
    if (!BossRow)
    {
        return;
    }

    RequestBossClassPreload(*BossRow);

    const float SafeCountdownSec = FMath::IsFinite(CountdownSec) ? FMath::Max(0.f, CountdownSec) : 0.f;
    const float Now = GetWorld()->GetTimeSeconds();

    // 클라이언트가 같은 서버 시간 기준으로 남은 카운트다운을 계산하도록 종료 시각을 복제한다.
    RGS->CountdownEndServerTime = Now + SafeCountdownSec;
    RGS->SetRaidState_ServerOnly(ELBRaidState::Countdown);
    if (bRestartingCountdown)
    {
        // 상태 값은 같아도 종료 시각은 바뀌었으므로 재시작 시에만 즉시 동기화를 요청한다.
        RGS->ForceNetUpdate();
    }

    // 중복 호출되더라도 기존 타이머를 지우고 하나만 예약한다.
    GetWorldTimerManager().ClearTimer(CountdownTimerHandle);

    // SetTimer는 0 이하에서 타이머를 해제하므로 0초 설정은 명시적으로 즉시 시작한다.
    if (SafeCountdownSec <= 0.f)
    {
        StartBattle();
        return;
    }

    GetWorldTimerManager().SetTimer(
        CountdownTimerHandle,
        this,
        &ALB_RaidGameMode::StartBattle,
        SafeCountdownSec,
        false
    );

    LBRaidDebug(
        GetWorld(),
        FString::Printf(TEXT("[RaidGM] Countdown started. %.1f sec"), SafeCountdownSec),
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

    // 카운트다운 타이머의 지연 호출이나 외부 중복 호출이 상태를 역행시키지 못하게 한다.
    if (RGS->RaidState != ELBRaidState::Countdown)
    {
        UE_LOG(
            LogLBRaidGameMode,
            Warning,
            TEXT("[RaidGM] StartBattle ignored in state %d."),
            static_cast<int32>(RGS->RaidState));
        return;
    }

    if (SpawnedBoss.IsValid())
    {
        UE_LOG(LogLBRaidGameMode, Warning, TEXT("[RaidGM] StartBattle ignored because a boss already exists."));
        return;
    }

    GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
    LBRaidDebug(GetWorld(), TEXT("[RaidGM] StartBattle called."), FColor::Green);

    // 보스 스폰이 실패하면 Battle 상태로 넘어가지 않는다.
    if (!SpawnBossFromData())
    {
        LBRaidDebug(GetWorld(), TEXT("[RaidGM] ERROR: Boss spawn failed. Battle not started."), FColor::Red, 10.f);
        // 로드/스폰 실패 후 만료된 Countdown 상태에 고착되지 않도록 재시도 가능한 Waiting으로 복구한다.
        RGS->CountdownEndServerTime = 0.f;
        RGS->SetRaidState_ServerOnly(ELBRaidState::Waiting);
        CancelBossClassPreload();
        return;
    }

    // 이전 레이드를 같은 월드에서 다시 시작하더라도 누적 통계와 MVP가 남지 않게 한다.
    ResetAllPlayerRaidStats_ServerOnly();

    // 전투 시작 시간을 복제해 UI 타이머와 클리어 타임 계산의 기준으로 사용한다.
    // 0초 카운트다운이 월드 시작 프레임에 실행되어도 "미시작(0)" sentinel과 충돌하지 않게 최소 양수로 저장한다.
    RGS->BattleStartServerTime = FMath::Max(GetWorld()->GetTimeSeconds(), UE_KINDA_SMALL_NUMBER);
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

    LBRaidDebug(
        GetWorld(),
        FString::Printf(TEXT("[RaidGM] Battle started. TimeLimit=%.1f"), RGS->TimeLimitSec),
        FColor::Green
    );
}

bool ALB_RaidGameMode::SpawnBossFromData()
{
    if (SpawnedBoss.IsValid())
    {
        UE_LOG(LogLBRaidGameMode, Warning, TEXT("[RaidGM] Duplicate boss spawn request ignored."));
        return false;
    }

    const FLBBossStatsRow* BossRow = FindValidatedBossRow(TEXT("SpawnBossFromData"));
    if (!BossRow)
    {
        return false;
    }

    // 정상 경로에서는 카운트다운 중 프리로드된 클래스를 O(1)로 가져온다.
    UClass* LoadedBossClass = BossRow->BossClass.Get();
    if (!LoadedBossClass)
    {
        // 비동기 요청이 아직 끝나지 않았거나 요청 생성에 실패한 예외 상황만 동기 fallback을 사용한다.
        // 전투 시작 안정성을 우선하되 정상 플레이에서는 package load hitch가 발생하지 않는다.
        UE_LOG(
            LogLBRaidGameMode,
            Warning,
            TEXT("[RaidGM] BossClass preload incomplete. Using synchronous fallback. Row=%s"),
            *BossRowName.ToString());
        LoadedBossClass = BossRow->BossClass.LoadSynchronous();
    }

    if (!LoadedBossClass || !LoadedBossClass->IsChildOf(ALB_BossCharacter::StaticClass()))
    {
        UE_LOG(LogLBRaidGameMode, Error, TEXT("[RaidGM] BossClass load/validation failed. Row=%s"), *BossRowName.ToString());
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
            LogLBRaidGameMode,
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
            LogLBRaidGameMode,
            Warning,
            TEXT("[RaidGM] BossSpawn tag not found. Spawn at world origin.")
        );
    }

    // 보스 CDO에서 캡슐 크기를 읽어 바닥 기준 스폰 위치를 캐릭터 중심 위치로 보정한다.
    const ALB_BossCharacter* BossCDO = Cast<ALB_BossCharacter>(LoadedBossClass->GetDefaultObject());

    if (bBossSpawnPointIsFloorLocation && BossCDO && BossCDO->GetCapsuleComponent())
    {
        const float CapsuleHalfHeight = BossCDO->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        SpawnLocation.Z += CapsuleHalfHeight;

        UE_LOG(
            LogLBRaidGameMode,
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
    ALB_BossCharacter* NewBoss = GetWorld()->SpawnActor<ALB_BossCharacter>(
        LoadedBossClass,
        SpawnTransform,
        Params);

    if (!NewBoss)
    {
        UE_LOG(LogLBRaidGameMode, Error, TEXT("[RaidGM] SpawnActor returned null."));
        return false;
    }

    SpawnedBoss = NewBoss;
    // Spawn된 액터의 UClass가 패키지를 참조하므로 프리로드 핸들의 강한 참조는 더 이상 필요 없다.
    BossClassLoadHandle.Reset();

    // 혹시라도 BP나 외부 설정에서 Scale이 꼬였을 경우 한 번 더 보정
    NewBoss->SetActorScale3D(FVector::OneVector);
    NewBoss->SetActorHiddenInGame(false);
    NewBoss->SetActorEnableCollision(true);

    // 블루프린트 설정 문제로 Mesh가 숨겨져 있더라도 테스트에서 보이도록 보정한다.
    if (USkeletalMeshComponent* BossMesh = NewBoss->GetMesh())
    {
        BossMesh->SetVisibility(true, true);
        BossMesh->SetHiddenInGame(false, true);
    }

    // 보스의 HP/사망 이벤트를 GameMode에 연결해 GameState 갱신과 승리 처리를 이어 준다.
    NewBoss->OnBossHPChanged.AddUniqueDynamic(this, &ALB_RaidGameMode::NotifyBossHPChanged);
    NewBoss->OnBossDied.AddUniqueDynamic(this, &ALB_RaidGameMode::NotifyBossDied);
    // 데이터 테이블의 수치로 보스 HP/마나를 최대치까지 채운다.
    NewBoss->InitializeBossStats_ServerOnly(BossRow->MaxHP, BossRow->MaxMana, BossRow->DEF);

    if (ALB_RaidGameState* RGS = GetLBRaidGameState())
    {
        const float SafeDefaultTimeLimit = FMath::IsFinite(DefaultTimeLimitSec) && DefaultTimeLimitSec > 0.f
            ? DefaultTimeLimitSec
            : LBFallbackRaidTimeLimitSec;
        const bool bHasValidBossTimeLimit = FMath::IsFinite(BossRow->TimeLimitSec) && BossRow->TimeLimitSec > 0.f;

        // 제한 시간은 전투 종료 타이머의 필수 입력이다. 손상된 행만 검증된 기본값으로 대체한다.
        RGS->TimeLimitSec = bHasValidBossTimeLimit ? BossRow->TimeLimitSec : SafeDefaultTimeLimit;
        if (!bHasValidBossTimeLimit)
        {
            UE_LOG(
                LogLBRaidGameMode,
                Warning,
                TEXT("[RaidGM] Invalid boss TimeLimitSec. Fallback=%.1f Row=%s"),
                RGS->TimeLimitSec,
                *BossRowName.ToString());
        }

        RGS->SetBossHP_ServerOnly(NewBoss->GetCurrentHP(), NewBoss->GetMaxHP());
    }

    LBRaidDebug(
        GetWorld(),
        FString::Printf(
            TEXT("[RaidGM] Boss spawned. Actor=%s HP=%.0f/%.0f DEF=%.0f Location=%s Scale=%s"),
            *NewBoss->GetName(),
            NewBoss->GetCurrentHP(),
            NewBoss->GetMaxHP(),
            // 마나 생기면 주석 해제 NewBoss->GetCurrentMana(),
            // 마나 생기면 주석 해제 NewBoss->GetMaxMana(),
            NewBoss->GetDEF(),
            *NewBoss->GetActorLocation().ToString(),
            *NewBoss->GetActorScale3D().ToString()
        ),
        FColor::Cyan,
        8.f
    );

    return true;
}

void ALB_RaidGameMode::NotifyBossHPChanged(float CurrentHP, float MaxHP)
{
    if (bRaidEnded)
    {
        return;
    }

    // BossBase의 내부 HP 변경을 GameState의 복제용 HP 값으로 옮긴다.
    if (ALB_RaidGameState* RGS = GetLBRaidGameState())
    {
        // Result 이후 남은 투사체/효과가 최종 UI 스냅샷을 변경하지 않도록 Battle에서만 수용한다.
        if (RGS->RaidState == ELBRaidState::Battle)
        {
            RGS->SetBossHP_ServerOnly(CurrentHP, MaxHP);
        }
    }
}

void ALB_RaidGameMode::NotifyBossDied()
{
    // 이미 다른 조건으로 종료되었다면 보스 사망 이벤트를 무시한다.
    ALB_RaidGameState* RGS = GetLBRaidGameState();
    if (bRaidEnded || !RGS || RGS->RaidState != ELBRaidState::Battle)
    {
        return;
    }

    LBRaidDebug(GetWorld(), TEXT("[RaidGM] Boss died. Victory."), FColor::Cyan, 8.f);
    EndRaid(true, ELBRaidEndReason::BossKilled);
}

void ALB_RaidGameMode::NotifyPlayerDied(AController* DeadController)
{
    // 종료 후 이벤트나 잘못된 컨트롤러 입력은 집계하지 않는다.
    ALB_RaidGameState* RGS = GetLBRaidGameState();
    if (bRaidEnded || !DeadController || !RGS || RGS->RaidState != ELBRaidState::Battle)
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
    bool bHasActivePlayer = false;

    if (GameState)
    {
        for (APlayerState* PS : GameState->PlayerArray)
        {
            const ALB_PlayerState* OtherPS = Cast<ALB_PlayerState>(PS);
            if (!OtherPS || OtherPS->IsOnlyASpectator() || OtherPS->IsInactive())
            {
                continue;
            }

            bHasActivePlayer = true;
            if (!OtherPS->IsDead())
            {
                bAllDead = false;
                break;
            }
        }
    }

    if (bHasActivePlayer && bAllDead)
    {
        LBRaidDebug(GetWorld(), TEXT("[RaidGM] All players dead. Defeat."), FColor::Red, 8.f);
        EndRaid(false, ELBRaidEndReason::AllDead);
    }
}

void ALB_RaidGameMode::HandleTimeLimitReached()
{
    // 보스 처치나 전멸로 이미 끝난 뒤라면 시간 초과 처리는 하지 않는다.
    ALB_RaidGameState* RGS = GetLBRaidGameState();
    if (bRaidEnded || !RGS || RGS->RaidState != ELBRaidState::Battle)
    {
        return;
    }

    LBRaidDebug(GetWorld(), TEXT("[RaidGM] Time limit reached. Defeat."), FColor::Red, 8.f);
    EndRaid(false, ELBRaidEndReason::TimeOut);
}

void ALB_RaidGameMode::ResetAllPlayerRaidStats_ServerOnly()
{
    if (!HasAuthority() || !GameState)
    {
        return;
    }

    for (APlayerState* PlayerState : GameState->PlayerArray)
    {
        if (ALB_PlayerState* LBPlayerState = Cast<ALB_PlayerState>(PlayerState))
        {
            LBPlayerState->ResetRaidStats_ServerOnly();
        }
    }
}

FLBRaidScoreboardData ALB_RaidGameMode::BuildRaidScoreboardData(const FLBRaidResultData& ResultData)
{
    FLBRaidScoreboardData ScoreboardData;
    ScoreboardData.bVictory = ResultData.bVictory;
    ScoreboardData.EndReason = ResultData.EndReason;
    ScoreboardData.ClearTimeSec = ResultData.ClearTimeSec;
    ScoreboardData.RankID = ResultData.RankID;

    if (!HasAuthority() || !GameState)
    {
        UE_LOG(LogLBRaidGameMode, Warning, TEXT("[RaidGM] Cannot build scoreboard without server authority and GameState."));
        return ScoreboardData;
    }

    struct FCapturedPlayerResult
    {
        ALB_PlayerState* PlayerState = nullptr;
        LBRaidMVP::FScoringInput ScoringInput;
    };

    TArray<FCapturedPlayerResult> CapturedPlayers;
    CapturedPlayers.Reserve(GameState->PlayerArray.Num());

    for (APlayerState* PlayerState : GameState->PlayerArray)
    {
        ALB_PlayerState* LBPlayerState = Cast<ALB_PlayerState>(PlayerState);
        if (!LBPlayerState)
        {
            continue;
        }

        // 이전 결과가 남아 있지 않도록 후보 포함 여부와 관계없이 먼저 해제한다.
        LBPlayerState->SetMVP_ServerOnly(false);

        if (LBPlayerState->IsOnlyASpectator() || LBPlayerState->IsInactive())
        {
            continue;
        }

        FCapturedPlayerResult& Captured = CapturedPlayers.AddDefaulted_GetRef();
        Captured.PlayerState = LBPlayerState;
        Captured.ScoringInput.RoleType = LBPlayerState->GetRoleType();
        Captured.ScoringInput.TotalDamageDealt = LBRaidMVP::SanitizeStat(LBPlayerState->GetTotalDamageDealt());
        Captured.ScoringInput.TotalHealingDone = LBRaidMVP::SanitizeStat(LBPlayerState->GetTotalHealingDone());
        Captured.ScoringInput.DeathCount = FMath::Max(0, LBPlayerState->GetDeathCount());
        Captured.ScoringInput.PlayerId = LBPlayerState->GetPlayerId();
    }

    TArray<LBRaidMVP::FScoringInput> ScoringInputs;
    ScoringInputs.Reserve(CapturedPlayers.Num());
    for (const FCapturedPlayerResult& Captured : CapturedPlayers)
    {
        ScoringInputs.Add(Captured.ScoringInput);
    }

    const float SafePrimaryWeight = FMath::IsFinite(MVPPrimaryWeight)
        ? FMath::Clamp(MVPPrimaryWeight, 0.f, 1.f)
        : LBRaidMVP::DefaultPrimaryWeight;
    const int32 MVPIndex = LBRaidMVP::SelectMVPIndex(ScoringInputs, SafePrimaryWeight);

    ScoreboardData.PlayerResults.Reserve(CapturedPlayers.Num());
    for (int32 Index = 0; Index < CapturedPlayers.Num(); ++Index)
    {
        const FCapturedPlayerResult& Captured = CapturedPlayers[Index];
        const bool bIsMVP = Index == MVPIndex;
        Captured.PlayerState->SetMVP_ServerOnly(bIsMVP);

        FLBPlayerFinalResult& PlayerResult = ScoreboardData.PlayerResults.AddDefaulted_GetRef();
        PlayerResult.PlayerName = Captured.PlayerState->GetPlayerNameText();
        PlayerResult.CharacterID = Captured.PlayerState->GetCharacterID();
        PlayerResult.RoleType = Captured.ScoringInput.RoleType;
        PlayerResult.TotalDamageDealt = Captured.ScoringInput.TotalDamageDealt;
        PlayerResult.TotalHealingDone = Captured.ScoringInput.TotalHealingDone;
        PlayerResult.DeathCount = Captured.ScoringInput.DeathCount;
        PlayerResult.bIsMVP = bIsMVP;
    }

    if (CapturedPlayers.IsValidIndex(MVPIndex))
    {
        const LBRaidMVP::FScoringContext Context = LBRaidMVP::BuildScoringContext(ScoringInputs);
        const LBRaidMVP::FScoringResult Score =
            LBRaidMVP::CalculateScore(CapturedPlayers[MVPIndex].ScoringInput, Context, SafePrimaryWeight);

        UE_LOG(
            LogLBRaidGameMode,
            Log,
            TEXT("[RaidGM] MVP selected. Player=%s Score=%.4f Players=%d"),
            *CapturedPlayers[MVPIndex].PlayerState->GetPlayerName(),
            Score.Score,
            CapturedPlayers.Num()
        );
    }
    else
    {
        UE_LOG(LogLBRaidGameMode, Log, TEXT("[RaidGM] No MVP selected because every contribution score was zero."));
    }

    return ScoreboardData;
}

void ALB_RaidGameMode::EndRaid(bool bVictory, ELBRaidEndReason EndReason)
{
    // 보스 사망, 전멸, 시간 초과가 동시에 들어와도 결과는 한 번만 확정한다.
    ALB_RaidGameState* RGS = GetLBRaidGameState();
    if (bRaidEnded || !RGS || RGS->RaidState != ELBRaidState::Battle)
    {
        return;
    }

    bRaidEnded = true;

    // 종료 이후 남아 있는 타이머가 추가 상태 변경을 만들지 않도록 모두 정리한다.
    GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
    GetWorldTimerManager().ClearTimer(TimeLimitTimerHandle);

    // 전투 시작 시간이 없다면 비정상 종료로 보고 클리어 시간을 0으로 기록한다.
    const float ClearTimeSec =
        RGS->BattleStartServerTime > 0.f
        ? FMath::Max(0.f, GetWorld()->GetTimeSeconds() - RGS->BattleStartServerTime)
        : 0.f;

    // GameState와 CSV 로그에 남길 최종 결과 데이터를 한 번에 구성한다.
    FLBRaidResultData Result;
    Result.bVictory = bVictory;
    Result.EndReason = EndReason;
    Result.ClearTimeSec = ClearTimeSec;
    Result.PlayerDeaths = GetTotalPlayerDeaths();
    Result.BossRemainingHPOnFail = bVictory ? 0.f : GetBossRemainingHP();
    Result.RankID = bVictory ? CalculateRank(ClearTimeSec) : NAME_None;

    // 개별 PlayerState의 최종 통계를 하나의 불변 결과 스냅샷으로 만든다.
    const FLBRaidScoreboardData ScoreboardData = BuildRaidScoreboardData(Result);

    // 결과/스코어보드/상태를 하나의 서버 트랜잭션으로 확정해 이벤트 순서와 단일 네트워크 flush를 보장한다.
    RGS->SetRaidOutcome_ServerOnly(Result, ScoreboardData);

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
    int64 TotalDeaths = 0;

    // GameState가 아직 없으면 집계할 PlayerArray도 없으므로 0을 반환한다.
    if (!GameState)
    {
        return 0;
    }

    // 레이드 전용 PlayerState만 골라 사망 횟수를 합산한다.
    for (APlayerState* PS : GameState->PlayerArray)
    {
        if (const ALB_PlayerState* LBPS = Cast<ALB_PlayerState>(PS);
            LBPS && !LBPS->IsOnlyASpectator() && !LBPS->IsInactive())
        {
            // 플레이어 수가 비정상적으로 많아도 결과 구조체의 int32를 wrap시키지 않는다.
            TotalDeaths = FMath::Min<int64>(TotalDeaths + FMath::Max(0, LBPS->GetDeathCount()), MAX_int32);
        }
    }

    return static_cast<int32>(TotalDeaths);
}

float ALB_RaidGameMode::GetBossRemainingHP() const
{
    // 보스 스폰 실패 후 종료될 수 있으므로 null이면 0으로 처리한다.
    const ALB_BossCharacter* Boss = SpawnedBoss.Get();
    if (!Boss)
    {
        return 0.f;
    }

    const float CurrentHP = Boss->GetCurrentHP();
    return FMath::IsFinite(CurrentHP) ? FMath::Max(0.f, CurrentHP) : 0.f;
}

FName ALB_RaidGameMode::CalculateRank(float ClearTimeSec) const
{
    if (!FMath::IsFinite(ClearTimeSec) || ClearTimeSec < 0.f)
    {
        UE_LOG(LogLBRaidGameMode, Warning, TEXT("[RaidGM] Invalid ClearTimeSec. RankID will be None. Value=%f"), ClearTimeSec);
        return NAME_None;
    }

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
        if (!RankRow
            || RankRow->RankID.IsNone()
            || !FMath::IsFinite(RankRow->ClearTimeSec)
            || RankRow->ClearTimeSec < 0.f
            || ClearTimeSec > RankRow->ClearTimeSec)
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
