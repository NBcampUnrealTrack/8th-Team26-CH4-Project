// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Base/LB_BaseRaidWidget.h"
#include "LB_TimerWidget.generated.h"


UCLASS()
class LEFTBEHIND_API ULB_TimerWidget : public ULB_BaseRaidWidget
{
	GENERATED_BODY()
	
public:

	// RaidGameState -> ... -> BattleHUDWidget에서 남은 시간 전달
	UFUNCTION(BlueprintCallable, Category="LB|Timer")
	void SetRemainingTime(float RemainingTime);

protected:

	// BP에서 mm:ss 출력
	UFUNCTION(BlueprintImplementableEvent, Category="LB|Timer")
	void BP_SetRemainingTime(int32 Minutes, int32 Seconds);
};
