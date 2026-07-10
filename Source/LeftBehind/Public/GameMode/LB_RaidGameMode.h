//LBRaidGameMode.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LB_RaidGameMode.generated.h"

class ALB_RaidGameState;
class ALB_BossCharacter;
class UDataTable;
enum class ELBRaidEndReason : uint8;
struct FStreamableHandle;
struct FLBBossStatsRow;
struct FLBRaidResultData;
struct FLBRaidScoreboardData;

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

    // 월드 종료 시 타이머, 보스 델리게이트, 비동기 로드 요청을 명시적으로 정리한다.
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Raid|Time", meta = (ClampMin = "0.0"))
    float CountdownSec = 5.f;

    // 보스 데이터가 없을 때 사용할 기본 제한 시간.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Raid|Time", meta = (ClampMin = "0.01"))
    float DefaultTimeLimitSec = 300.f;

    // MVP 점수에서 각 역할의 주 임무(딜러=피해, 힐러=회복)가 차지하는 비율이다.
    // 나머지 비율은 보조 기여도에 사용한다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Raid|MVP", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MVPPrimaryWeight = 0.8f;

    // GameMode가 월드 소유 액터의 수명을 연장할 이유가 없으므로 약한 참조로 보관해
    // 레벨 전환/파괴 시 불필요한 강한 참조와 stale pointer 위험을 동시에 줄인다.
    UPROPERTY(Transient)
    TWeakObjectPtr<ALB_BossCharacter> SpawnedBoss;

    // 반복 GetGameState 캐스트를 피하되 월드 수명은 침범하지 않도록 약한 참조로 캐시한다.
    UPROPERTY(Transient)
    TWeakObjectPtr<ALB_RaidGameState> CachedRaidGameState;

    // 카운트다운 종료 후 StartBattle을 호출하는 타이머.
    FTimerHandle CountdownTimerHandle;
    // 전투 제한 시간이 끝났을 때 패배 처리를 호출하는 타이머.
    FTimerHandle TimeLimitTimerHandle;

    // EndRaid가 중복 호출되는 것을 막는 플래그.
    bool bRaidEnded = false;

    // 카운트다운 동안 SoftClass를 미리 읽어 전투 시작 순간의 동기 로드 hitch를 줄인다.
    TSharedPtr<FStreamableHandle> BossClassLoadHandle;

    // 현재 월드의 레이드 전용 GameState를 가져온다.
    ALB_RaidGameState* GetLBRaidGameState();

    // 보스 행의 필수 계약(행/클래스/최대 HP)을 전투 진입 전에 검증한다.
    const FLBBossStatsRow* FindValidatedBossRow(const TCHAR* Context) const;
    // 검증된 SoftClass를 카운트다운과 병렬로 비동기 프리로드한다.
    void RequestBossClassPreload(const FLBBossStatsRow& BossRow);
    // 월드 종료 또는 재요청 시 남아 있는 비동기 핸들을 안전하게 해제한다.
    void CancelBossClassPreload();

    // 카운트다운 종료 후 보스를 스폰하고 Battle 상태로 전환한다.
    void StartBattle();
    // 데이터 테이블과 스폰 태그를 이용해 보스를 생성하고 GameState 초기 HP를 설정한다.
    bool SpawnBossFromData();
    // 제한 시간이 끝났을 때 레이드를 패배로 종료한다.
    void HandleTimeLimitReached();

    // 전투 시작 직전에 모든 참가 플레이어의 이전 레이드 통계를 초기화한다.
    void ResetAllPlayerRaidStats_ServerOnly();

    // 서버 PlayerArray를 스냅샷으로 만들고 역할별 MVP 한 명을 선정한다.
    FLBRaidScoreboardData BuildRaidScoreboardData(const FLBRaidResultData& ResultData);

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
