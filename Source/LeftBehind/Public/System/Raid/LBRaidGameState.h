//LBRaidGameState.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "System/Raid/LBRaidTypes.h"
#include "LBRaidGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLBRaidStateChanged, ELBRaidState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLBBossHPChanged, float, CurrentHP, float, MaxHP);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLBRaidResultChanged, const FLBRaidResultData&, ResultData);

UCLASS()
class LEFTBEHIND_API ALBRaidGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    ALBRaidGameState();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(BlueprintAssignable)
    FOnLBRaidStateChanged OnRaidStateChanged;

    UPROPERTY(BlueprintAssignable)
    FOnLBBossHPChanged OnBossHPChanged;

    UPROPERTY(BlueprintAssignable)
    FOnLBRaidResultChanged OnRaidResultChanged;

    UPROPERTY(ReplicatedUsing=OnRep_RaidState, BlueprintReadOnly, Category="LB|Raid")
    ELBRaidState RaidState = ELBRaidState::Waiting;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="LB|Raid")
    float CountdownEndServerTime = 0.f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="LB|Raid")
    float BattleStartServerTime = 0.f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="LB|Raid")
    float TimeLimitSec = 300.f;

    UPROPERTY(ReplicatedUsing=OnRep_BossHP, BlueprintReadOnly, Category="LB|Raid")
    float BossCurrentHP = 0.f;

    UPROPERTY(ReplicatedUsing=OnRep_BossHP, BlueprintReadOnly, Category="LB|Raid")
    float BossMaxHP = 1.f;

    UPROPERTY(ReplicatedUsing=OnRep_RaidResult, BlueprintReadOnly, Category="LB|Raid")
    FLBRaidResultData RaidResult;

    UFUNCTION(BlueprintPure, Category="LB|Raid")
    float GetCountdownRemaining() const;

    UFUNCTION(BlueprintPure, Category="LB|Raid")
    float GetBattleElapsed() const;

    UFUNCTION(BlueprintPure, Category="LB|Raid")
    float GetBattleRemaining() const;

    UFUNCTION(BlueprintPure, Category="LB|Raid")
    float GetBossHPRatio() const;

    void SetRaidState_ServerOnly(ELBRaidState NewState);
    void SetBossHP_ServerOnly(float CurrentHP, float MaxHP);
    void SetRaidResult_ServerOnly(const FLBRaidResultData& NewResult);

protected:
    UFUNCTION()
    void OnRep_RaidState();

    UFUNCTION()
    void OnRep_BossHP();

    UFUNCTION()
    void OnRep_RaidResult();
};