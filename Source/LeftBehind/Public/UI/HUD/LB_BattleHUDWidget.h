// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Base/LB_BaseRaidWidget.h"
#include "LB_BattleHUDWidget.generated.h"

// 패널 조합
// GameState에서 오는 데이터(보스, 타이머)는 BattleHUD에서 관리 -- 디자인상 타이머는 BossHPWidgetd으로 이동
// ASC 데이터는 각자 위젯에서 직접 관리

class ULB_BossHPWidget;
class ULB_PlayerStatusWidget;
class ULB_PartyStatusWidget;

UCLASS()
class LEFTBEHIND_API ULB_BattleHUDWidget : public ULB_BaseRaidWidget
{
	GENERATED_BODY()
	
protected:

	virtual void HandleBossHPChanged(float CurrentHP, float MaxHP) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<ULB_BossHPWidget> BossHPWidget;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<ULB_PlayerStatusWidget> PlayerStatusWidget;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<ULB_PartyStatusWidget> PartyWidget;
};
