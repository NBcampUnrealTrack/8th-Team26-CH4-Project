// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Base/LB_BaseRaidWidget.h"
#include "LB_BossHPWidget.generated.h"

/**
 * 
 */
UCLASS()
class LEFTBEHIND_API ULB_BossHPWidget : public ULB_BaseRaidWidget
{
	GENERATED_BODY()
	
public:

	// RaidGameState -> ... -> BattleHUDWidget에서 보스 HP 전달
	UFUNCTION(BlueprintCallable, Category="LB|Boss")
	void SetBossHP(float CurrentHP, float MaxHP);

protected:

	// BP에서 ProgressBar, HP Text 등 갱신
	UFUNCTION(BlueprintImplementableEvent, Category="LB|Boss")
	void BP_SetBossHP(float CurrentHP, float MaxHP, float HPRatio);
};
