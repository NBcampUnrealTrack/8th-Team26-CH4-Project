//LBRaidGameMode.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "System/Character/LBCharacterTypes.h"
#include "LB_RaidGameMode.generated.h"

class ALB_RaidGameState;
class ALB_BossCharacter;
class ALB_PlayerController;
class APlayerController;
class UDataTable;
class ULevelSequence;
class UWorld;
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

#if WITH_DEV_AUTOMATION_TESTS
    friend class FLBRaidReturnToMenuContractTest;
    friend class FLBRaidPausePolicyTest;
#endif

public:
    ALB_RaidGameMode();

    // 캐릭터 선택 맵이 전달한 파티 인원을 받아 모든 소유 클라이언트 준비 전 조기 시작을 막는다.
    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

    // 레이드 GameState를 초기화하고 설정에 따라 자동 카운트다운을 시작한다.
    virtual void BeginPlay() override;
    
    // 월드 종료 시 타이머, 보스 델리게이트, 비동기 로드 요청을 명시적으로 정리한다.
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // 레이드 입장 중에는 캐릭터 Pawn 생성을 미루고 Battle 진입 뒤 참가시키도록 한다.
    virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

    virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;

    // 소유 클라이언트가 레이드 PlayerController/PlayerState/GameState 준비를 마쳤음을 서버에 기록한다.
    void NotifyRaidClientReady(ALB_PlayerController* ReadyController);

    // 표준 알림이 서버 transition 중 도착한 원격 플레이어의 seamless 교체를 최종 맵에서 복구한다.
    void RecoverRaidPlayerAfterClientLoaded(APlayerController* LoadedController);
    
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

    // 결과 화면에서 로컬 Listen Host(또는 Standalone)만 메인 메뉴 복귀를 요청할 수 있다.
    UFUNCTION(BlueprintPure, Category = "LB|Raid|Travel")
    bool CanReturnToMainMenu(const APlayerController* RequestingController) const;

    // 검증을 통과한 호스트 요청으로 파티 전체를 메인 메뉴에 seamless travel한다.
    bool TryReturnToMainMenu(APlayerController* RequestingController);

    // 활성 레이드에서 로컬 Listen Host(또는 Standalone)가 전역 일시정지를 제어할 수 있는지 검사한다.
    UFUNCTION(BlueprintPure, Category = "LB|Raid|Pause")
    bool CanSetHostPause(const APlayerController* RequestingController) const;

    // 방장의 전역 일시정지를 설정한다. 해제는 단계가 바뀐 뒤에도 소유한 Pause를 정리할 수 있다.
    bool TrySetHostPause(APlayerController* RequestingController, bool bShouldPause);

    // 활성 레이드에서 로컬 Listen Host(또는 Standalone)가 파티 복귀를 요청할 수 있는지 검사한다.
    UFUNCTION(BlueprintPure, Category = "LB|Raid|Travel")
    bool CanAbortRaidToRoom(const APlayerController* RequestingController) const;

    // 활성 레이드를 중단하고 파티 전체를 메인 메뉴 방으로 seamless travel한다.
    bool TryAbortRaidToRoom(APlayerController* RequestingController);

    UFUNCTION(BlueprintPure, Category = "LB|Raid|Travel")
    TSoftObjectPtr<UWorld> GetMainMenuMap() const { return MainMenuMap; }

protected:
    
    UPROPERTY(EditDefaultsOnly)
    TObjectPtr<UDataTable> CharacterDataTable;
    
    const FLBCharacterData* FindCharacterData(ELBCharacterID CharacterID) const;
    
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
    float CountdownSec = 13.5f;

    // 모든 클라이언트가 준비된 뒤 서버가 재생할 보스 등장 시퀀스.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Raid|Cinematic")
    TSoftObjectPtr<ULevelSequence> RaidIntroSequence;

    // 비정상 클라이언트 하나가 준비 RPC를 보내지 못해 레이드 전체가 영구 정지하지 않도록 하는 상한 시간.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Raid|Cinematic", meta = (ClampMin = "0.1"))
    float RaidClientReadyTimeoutSec = 30.f;

    // 보스 데이터가 없을 때 사용할 기본 제한 시간.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Raid|Time", meta = (ClampMin = "0.01"))
    float DefaultTimeLimitSec = 300.f;

    // MVP 점수에서 각 역할의 주 임무(딜러=피해, 힐러=회복)가 차지하는 비율이다.
    // 나머지 비율은 보조 기여도에 사용한다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Raid|MVP", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MVPPrimaryWeight = 0.8f;

    // seamless travel 목적지. 파티의 코드네임/역할 선택은 유지하고 레이드 통계만 초기화한다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Raid|Travel")
    TSoftObjectPtr<UWorld> MainMenuMap;

    // GameMode가 월드 소유 액터의 수명을 연장할 이유가 없으므로 약한 참조로 보관해
    // 레벨 전환/파괴 시 불필요한 강한 참조와 stale pointer 위험을 동시에 줄인다.
    UPROPERTY(Transient)
    TWeakObjectPtr<ALB_BossCharacter> SpawnedBoss;

    // 반복 GetGameState 캐스트를 피하되 월드 수명은 침범하지 않도록 약한 참조로 캐시한다.
    UPROPERTY(Transient)
    TWeakObjectPtr<ALB_RaidGameState> CachedRaidGameState;

    // 카운트다운 종료 후 StartBattle을 호출하는 타이머.
    FTimerHandle CountdownTimerHandle;
    // 준비 RPC가 누락되어도 컷씬/카운트다운을 시작하는 안전 타이머.
    FTimerHandle RaidClientReadyTimeoutTimerHandle;
    // 전투 제한 시간이 끝났을 때 패배 처리를 호출하는 타이머.
    FTimerHandle TimeLimitTimerHandle;

    // EndRaid가 중복 호출되는 것을 막는 플래그.
    bool bRaidEnded = false;

    // 빠른 연속 클릭이나 중복 콜백이 ServerTravel을 여러 번 시작하지 못하게 한다.
    bool bReturnTravelInProgress = false;

    // 캐릭터 선택 맵을 떠날 때 확정된 파티 인원. 0이면 현재 서버 인원으로 대체한다.
    int32 ExpectedRaidPlayerCount = 0;

    // 준비 RPC 중복과 컷씬/카운트다운 중복 시작을 막는다.
    TSet<TWeakObjectPtr<ALB_PlayerController>> RaidReadyControllers;
    bool bRaidBeginPlayInitialized = false;
    bool bRaidStartTriggered = false;

    // 컷씬 동안 SoftClass를 미리 읽어 전투 시작 순간의 동기 로드 hitch를 줄인다.
    TSharedPtr<FStreamableHandle> BossClassLoadHandle;
    TSharedPtr<FStreamableHandle> PlayerClassLoadHandle;

    // 현재 월드의 레이드 전용 GameState를 가져온다.
    ALB_RaidGameState* GetLBRaidGameState();

    // MainMenuMap soft object path를 ServerTravel에 사용할 유효한 long package name으로 변환한다.
    bool GetMainMenuMapPackageName(FString& OutPackageName) const;

    // 요청자가 이 서버 프로세스의 로컬 Listen Host 또는 Standalone 컨트롤러인지 검사한다.
    bool IsValidLocalHostRequest(const APlayerController* RequestingController) const;

    // 결과 복귀와 활성 레이드 중단이 공유하는 seamless travel 및 중복 실행 가드 경로.
    bool StartMainMenuTravel(const TCHAR* RequestContext);

    // 보스 행의 필수 계약(행/클래스/최대 HP)을 전투 진입 전에 검증한다.
    const FLBBossStatsRow* FindValidatedBossRow(const TCHAR* Context) const;
    // 검증된 SoftClass를 카운트다운과 병렬로 비동기 프리로드한다.
    void RequestBossClassPreload(const FLBBossStatsRow& BossRow);
    // 월드 종료 또는 재요청 시 남아 있는 비동기 핸들을 안전하게 해제한다.
    void CancelBossClassPreload();
    void RequestRaidPlayerClassPreload();
    void CancelRaidPlayerClassPreload();

    // 준비 완료 인원수를 검사하고 컷씬과 카운트다운을 같은 서버 프레임에 시작한다.
    void TryStartRaidAfterClientReady();
    void ForceStartRaidAfterClientReadyTimeout();
    void ResetRaidIntroSequenceForManualStart();
    void PlayRaidIntroSequence();
    bool SpawnRaidPlayersForBattle();

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
