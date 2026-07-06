// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Base/LB_BaseRaidWidget.h"
#include "LB_CountdownWidget.generated.h"

// 레이드 시작 전 카운트다운
// BaseRaidWidget을 통해 RaidState 이벤트를 받음 -- Countdown 상태 동안만 타이머 갱신
UCLASS()
class LEFTBEHIND_API ULB_CountdownWidget : public ULB_BaseRaidWidget
{
	GENERATED_BODY()
	
protected:

	virtual void NativeDestruct() override;
	
	virtual void BP_OnRaidStateChanged_Implementation(ELBRaidState NewState) override;

	// 숫자가 바뀔 때만 호출
	UFUNCTION(BlueprintImplementableEvent, Category="LB|Countdown")
	void BP_UpdateCountdown(int32 RemainingSeconds);

private:

	void StartCountdownTimer();
	void StopCountdownTimer();
	
	UFUNCTION()
	void UpdateCountdown();
	
	FTimerHandle CountdownTimerHandle;
	int32 CachedSecond = INDEX_NONE;
};
