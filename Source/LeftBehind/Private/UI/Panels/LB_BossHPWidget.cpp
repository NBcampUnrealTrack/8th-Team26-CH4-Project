// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Panels/LB_BossHPWidget.h"
#include "UI/Panels/LB_TimerWidget.h"
#include "GameState/LB_RaidGameState.h"

void ULB_BossHPWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	if (!CachedRaidGameState || !TimerWidget) return;
	
	TimerWidget->SetRemainingTime(CachedRaidGameState->GetBattleRemaining());
}

void ULB_BossHPWidget::SetBossHP(float CurrentHP, float MaxHP)
{
	const float Ratio = (MaxHP > 0.f) ? CurrentHP / MaxHP : 0.f;

	BP_SetBossHP(CurrentHP, MaxHP, Ratio);
}