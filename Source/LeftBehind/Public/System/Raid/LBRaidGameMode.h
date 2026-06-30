//LBRaidGameMode.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "System/Raid/LBRaidTypes.h"
#include "System/Raid/LBRaidDataRows.h"
#include "LBRaidGameMode.generated.h"

class ALBRaidGameState;
class ALBRaidBossBase;
class UDataTable;

UCLASS()
class LEFTBEHIND_API ALBRaidGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ALBRaidGameMode();

    virtual void BeginPlay() override;

    UFUNCTION(BlueprintCallable, Category = "LB|Raid")
    void StartCountdown();

    UFUNCTION(BlueprintCallable, Category = "LB|Raid")
    void NotifyBossHPChanged(float CurrentHP, float MaxHP);

    UFUNCTION(BlueprintCallable, Category = "LB|Raid")
    void NotifyBossDied();

    UFUNCTION(BlueprintCallable, Category = "LB|Raid")
    void NotifyPlayerDied(AController* DeadController);

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Raid")
    bool bAutoStartOnBeginPlay = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Raid|Data")
    TObjectPtr<UDataTable> BossStatsTable;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Raid|Data")
    FName BossRowName = "Boss_Proto_Test";

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Raid|Data")
    TObjectPtr<UDataTable> RankDataTable;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Raid|Spawn")
    FName BossSpawnTag = "BossSpawn";

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Raid|Time")
    float CountdownSec = 5.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Raid|Time")
    float DefaultTimeLimitSec = 300.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Raid|Debug")
    bool bDebugAutoKillBoss = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Raid|Debug")
    float DebugAutoKillDelaySec = 3.f;

    UPROPERTY()
    TObjectPtr<ALBRaidBossBase> SpawnedBoss;

    FTimerHandle CountdownTimerHandle;
    FTimerHandle TimeLimitTimerHandle;
    FTimerHandle DebugAutoKillTimerHandle;

    bool bRaidEnded = false;

    ALBRaidGameState* GetLBRaidGameState() const;

    void StartBattle();
    bool SpawnBossFromData();
    void HandleTimeLimitReached();

    void DebugKillBoss_ServerOnly();

    void EndRaid(bool bVictory, ELBRaidEndReason EndReason);

    int32 GetTotalPlayerDeaths() const;
    float GetBossRemainingHP() const;
    FName CalculateRank(float ClearTimeSec) const;
    void WriteRaidLog(const FLBRaidResultData& ResultData) const;
};