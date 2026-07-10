// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Result/LB_RaidScoreSlotWidget.h"

void ULB_RaidScoreSlotWidget::SetPlayerResult(const FLBPlayerFinalResult& PlayerResult)
{
	BP_UpdatePlayerResult(PlayerResult);
}
