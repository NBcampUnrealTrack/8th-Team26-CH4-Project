// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Result/LB_RaidScoreboardWidget.h"
#include "Components/HorizontalBox.h"
#include "System/Raid/LBRaidTypes.h"
#include "UI/Result/LB_RaidScoreSlotWidget.h"

void ULB_RaidScoreboardWidget::HandleRaidScoreboardChanged(const FLBRaidScoreboardData& ScoreboardData)
{
	Super::HandleRaidScoreboardChanged(ScoreboardData);
	
	CachedScoreboardData = ScoreboardData;
	
	RefreshScoreboard();
}

void ULB_RaidScoreboardWidget::RefreshScoreboard()
{
	if (!PlayerResultContainer) return;
	
	PlayerResultContainer->ClearChildren();
	
	for (const FLBPlayerFinalResult& PlayerResult : CachedScoreboardData.PlayerResults)
	{
		ULB_RaidScoreSlotWidget* ResultSlot = 
			CreateWidget<ULB_RaidScoreSlotWidget>(GetOwningPlayer(), RaidScoreSlotClass);
		
		if (!ResultSlot) continue;
		
		ResultSlot->SetPlayerResult(PlayerResult);
		
		PlayerResultContainer->AddChild(ResultSlot);
	}
}
