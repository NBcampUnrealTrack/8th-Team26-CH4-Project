//LBRaidGameState.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "System/Raid/LBRaidTypes.h"
#include "LB_RaidGameState.generated.h"

// 레이드 상태가 Waiting/Countdown/Battle/Result로 바뀔 때 UI가 반응할 수 있게 한다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLBRaidStateChanged, ELBRaidState, NewState);
// 보스 HP 표시용 이벤트. GameMode가 BossBase의 HP 변경을 받아 GameState에 기록하면 이 이벤트가 흐른다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLBBossHPChanged, float, CurrentHP, float, MaxHP);
// 레이드 결과가 확정되었을 때 결과 화면이 구독하는 이벤트.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLBRaidResultChanged, const FLBRaidResultData&, ResultData);

// 모든 클라이언트가 읽어야 하는 레이드 진행 상태를 복제하는 GameState.
UCLASS()
class LEFTBEHIND_API ALB_RaidGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    ALB_RaidGameState();

    // 레이드 상태, 시간, 보스 HP, 결과 데이터를 복제 대상으로 등록한다.
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // 상태 변경을 블루프린트/UI에 전달한다.
    UPROPERTY(BlueprintAssignable)
    FOnLBRaidStateChanged OnRaidStateChanged;

    // 보스 HP 변경을 블루프린트/UI에 전달한다.
    UPROPERTY(BlueprintAssignable)
    FOnLBBossHPChanged OnBossHPChanged;

    // 최종 결과 변경을 블루프린트/UI에 전달한다.
    UPROPERTY(BlueprintAssignable)
    FOnLBRaidResultChanged OnRaidResultChanged;

    // 현재 레이드 단계. RepNotify로 상태 변경 이벤트와 디버그 표시를 실행한다.
    UPROPERTY(ReplicatedUsing=OnRep_RaidState, BlueprintReadOnly, Category="LB|Raid")
    ELBRaidState RaidState = ELBRaidState::Waiting;

    // 서버 시간 기준 카운트다운 종료 시각. 클라이언트가 남은 시간을 동일하게 계산한다.
    UPROPERTY(Replicated, BlueprintReadOnly, Category="LB|Raid")
    float CountdownEndServerTime = 0.f;

    // 서버 시간 기준 전투 시작 시각. 경과 시간과 클리어 시간을 계산하는 기준이다.
    UPROPERTY(Replicated, BlueprintReadOnly, Category="LB|Raid")
    float BattleStartServerTime = 0.f;

    // 현재 전투 제한 시간. 보스 데이터 테이블 값으로 갱신될 수 있다.
    UPROPERTY(Replicated, BlueprintReadOnly, Category="LB|Raid")
    float TimeLimitSec = 300.f;

    // UI에 표시할 보스 현재 HP.
    UPROPERTY(ReplicatedUsing=OnRep_BossHP, BlueprintReadOnly, Category="LB|Raid")
    float BossCurrentHP = 0.f;

    // UI에 표시할 보스 최대 HP. 0 나눗셈 방지를 위해 기본값은 1이다.
    UPROPERTY(ReplicatedUsing=OnRep_BossHP, BlueprintReadOnly, Category="LB|Raid")
    float BossMaxHP = 1.f;

    // 최종 결과 데이터. Result 상태로 넘어갈 때 함께 갱신된다.
    UPROPERTY(ReplicatedUsing=OnRep_RaidResult, BlueprintReadOnly, Category="LB|Raid")
    FLBRaidResultData RaidResult;

    // 카운트다운 상태일 때 서버 시간 기준 남은 초를 반환한다.
    UFUNCTION(BlueprintPure, Category="LB|Raid")
    float GetCountdownRemaining() const;

    // 전투 시작 이후 흐른 시간을 반환한다.
    UFUNCTION(BlueprintPure, Category="LB|Raid")
    float GetBattleElapsed() const;

    // Battle 상태일 때 제한 시간에서 경과 시간을 뺀 남은 초를 반환한다.
    UFUNCTION(BlueprintPure, Category="LB|Raid")
    float GetBattleRemaining() const;

    // 보스 HP 비율을 0~1 범위 값으로 계산한다.
    UFUNCTION(BlueprintPure, Category="LB|Raid")
    float GetBossHPRatio() const;

    // 서버에서 레이드 상태를 바꾸고 변경 이벤트를 즉시 발생시킨다.
    void SetRaidState_ServerOnly(ELBRaidState NewState);
    // 서버에서 보스 HP 표시 값을 갱신한다.
    void SetBossHP_ServerOnly(float CurrentHP, float MaxHP);
    // 서버에서 최종 결과 데이터를 확정한다.
    void SetRaidResult_ServerOnly(const FLBRaidResultData& NewResult);

protected:
    // RaidState 복제 후 상태 변경 이벤트와 디버그 로그를 실행한다.
    UFUNCTION()
    void OnRep_RaidState();

    // BossCurrentHP/BossMaxHP 복제 후 HP 변경 이벤트와 디버그 로그를 실행한다.
    UFUNCTION()
    void OnRep_BossHP();

    // RaidResult 복제 후 결과 이벤트와 디버그 로그를 실행한다.
    UFUNCTION()
    void OnRep_RaidResult();
};
