// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/HUD/LB_BattleHUDWidget.h"

#include "GameState/LB_RaidGameState.h"

#include "UI/Panels/LB_BossHPWidget.h"
#include "UI/Panels/LB_PlayerStatusWidget.h"
#include "UI/Panels/LB_PartyStatusWidget.h"

void ULB_BattleHUDWidget::HandleBossHPChanged(float CurrentHP, float MaxHP)
{
	Super::HandleBossHPChanged(CurrentHP, MaxHP);
	
	if (BossHPWidget)
	{
		BossHPWidget->SetBossHP(CurrentHP, MaxHP);
	}
}
