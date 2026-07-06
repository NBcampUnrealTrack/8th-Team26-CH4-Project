//LBRaidGameMode.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "System/Raid/LBRaidTypes.h"
#include "System/Raid/LBRaidDataRows.h"
#include "Characters/Boss/LB_BossCharacter.h"
#include "LB_RaidGameMode.generated.h"

class ALB_RaidGameState;
class ALB_BossCharacter;
class UDataTable;

// 레이드 전체 진행을 서버에서 제어하는 GameMode.
// 카운트다운, 보스 스폰, 승패 판정, 랭크 계산, 결과 로그 기록을 담당한다.
UCLASS()
class LEFTBEHIND_API ALB_RaidGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ALB_RaidGameMode();

    // 레이드 GameState를 초기화하고 설정에 따라 자동 카운트다운을 시작한다.
    virtual void BeginPlay() override;

    // Waiting 상태에서 Countdown 상태로 전환하고 전투 시작 타이머를 예약한다.
    UFUNCTION(BlueprintCallable, Category = "LB|Raid")
    void StartCountdown();

    // 보스 액터의 HP 변경 이벤트를 받아 GameState의 복제용 HP 값으로 전달한다.
    UFUNCTION(BlueprintCallable, Category = "LB|Raid")
    void NotifyBossHPChanged(float CurrentHP, float MaxHP);

    // 보스 사망 이벤트를 받아 레이드를 승리로 종료한다.
    UFUNCTION(BlueprintCallable, Category = "LB|Raid")
    void NotifyBossDied();

    // 플레이어 사망을 PlayerState에 기록하고 모든 플레이어 사망 여부를 검사한다.
    UFUNCTION(BlueprintCallable, Category = "LB|Raid")
    void NotifyPlayerDied(AController* DeadController);

protected:
    // BeginPlay에서 자동으로 카운트다운을 시작할지 여부.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Raid")
    bool bAutoStartOnBeginPlay = true;

    // 보스 스탯과 보스 클래스 정보를 담은 데이터 테이블.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Raid|Data")
    TObjectPtr<UDataTable> BossStatsTable;
    


    // BossStatsTable에서 사용할 행 이름.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Raid|Data")
    FName BossRowName = "Boss_Proto_Test";

    // 클리어 시간별 랭크/보상 정보를 담은 데이터 테이블.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Raid|Data")
    TObjectPtr<UDataTable> RankDataTable;

    // 보스 스폰 위치로 사용할 액터 태그.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Raid|Spawn")
    FName BossSpawnTag = "BossSpawn";

    // true면 스폰 포인트를 바닥 위치로 보고 보스 캡슐 반높이만큼 Z를 올린다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Raid|Spawn")
    bool bBossSpawnPointIsFloorLocation = true;

    // 전투 시작 전 카운트다운 시간.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Raid|Time")
    float CountdownSec = 5.f;

    // 보스 데이터가 없을 때 사용할 기본 제한 시간.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Raid|Time")
    float DefaultTimeLimitSec = 300.f;

    // 현재 레이드에서 스폰된 보스 액터 참조.
    UPROPERTY()
    TObjectPtr<ALB_BossCharacter> SpawnedBoss;

    // 카운트다운 종료 후 StartBattle을 호출하는 타이머.
    FTimerHandle CountdownTimerHandle;
    // 전투 제한 시간이 끝났을 때 패배 처리를 호출하는 타이머.
    FTimerHandle TimeLimitTimerHandle;

    // EndRaid가 중복 호출되는 것을 막는 플래그.
    bool bRaidEnded = false;

    // 현재 월드의 레이드 전용 GameState를 가져온다.
    ALB_RaidGameState* GetLBRaidGameState() const;

    // 카운트다운 종료 후 보스를 스폰하고 Battle 상태로 전환한다.
    void StartBattle();
    // 데이터 테이블과 스폰 태그를 이용해 보스를 생성하고 GameState 초기 HP를 설정한다.
    bool SpawnBossFromData();
    // 제한 시간이 끝났을 때 레이드를 패배로 종료한다.
    void HandleTimeLimitReached();

    // 승패 결과를 확정하고 타이머 정리, GameState 갱신, 로그 기록을 수행한다.
    void EndRaid(bool bVictory, ELBRaidEndReason EndReason);

    // 모든 PlayerState의 사망 횟수를 합산한다.
    int32 GetTotalPlayerDeaths() const;
    // 실패 결과 기록을 위해 현재 보스 잔여 HP를 읽는다.
    float GetBossRemainingHP() const;
    // 클리어 시간을 기준으로 가장 적절한 랭크 ID를 찾는다.
    FName CalculateRank(float ClearTimeSec) const;
    // 레이드 결과를 Saved/RaidLogs/RaidResults.csv에 누적 기록한다.
    void WriteRaidLog(const FLBRaidResultData& ResultData) const;
};
