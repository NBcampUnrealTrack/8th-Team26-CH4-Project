// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/HUD/LB_RaidHUDWidget.h"
#include "UI/HUD/LB_BattleHUDWidget.h"
#include "UI/Popup/LB_CountdownWidget.h"
#include "UI/Popup/LB_RaidResultWidget.h"
#include "UI/Result/LB_RaidScoreboardWidget.h"
#include "Components/Overlay.h"

void ULB_RaidHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ShowCountdown();
}

void ULB_RaidHUDWidget::HandleRaidStateChanged(ELBRaidState NewState)
{
	Super::HandleRaidStateChanged(NewState);
	
	switch (NewState)
	{
	case ELBRaidState::Waiting:
		ShowWaiting();
		break;

	case ELBRaidState::Countdown:
		ShowCountdown();
		break;

	case ELBRaidState::Battle:
		ShowBattle();
		break;

	case ELBRaidState::Result:
		ShowResult();
		break;

	default:
		break;
	}
}

void ULB_RaidHUDWidget::HandleRaidResultChanged(const FLBRaidResultData& ResultData)
{
	Super::HandleRaidResultChanged(ResultData);
}

void ULB_RaidHUDWidget::ShowWaiting() const
{
	if (BattleHUD)       BattleHUD->SetVisibility(ESlateVisibility::Hidden);
	if (CountdownWidget) CountdownWidget->SetVisibility(ESlateVisibility::Hidden);
	if (ResultWidget)    ResultWidget->SetVisibility(ESlateVisibility::Hidden);
	if (ScoreboardWidget) ScoreboardWidget->SetVisibility(ESlateVisibility::Hidden);
}

void ULB_RaidHUDWidget::ShowCountdown() const
{
	if (BattleHUD)       BattleHUD->SetVisibility(ESlateVisibility::Hidden);
	if (CountdownWidget) CountdownWidget->SetVisibility(ESlateVisibility::Visible);
	if (ResultWidget)    ResultWidget->SetVisibility(ESlateVisibility::Hidden);
	if (ScoreboardWidget) ScoreboardWidget->SetVisibility(ESlateVisibility::Hidden);
}

void ULB_RaidHUDWidget::ShowBattle() const
{
	if (BattleHUD)       BattleHUD->SetVisibility(ESlateVisibility::Visible);
	if (CountdownWidget) CountdownWidget->SetVisibility(ESlateVisibility::Hidden);
	if (ResultWidget)    ResultWidget->SetVisibility(ESlateVisibility::Hidden);
	if (ScoreboardWidget) ScoreboardWidget->SetVisibility(ESlateVisibility::Hidden);
}

void ULB_RaidHUDWidget::ShowResult() const
{
	if (BattleHUD)       BattleHUD->SetVisibility(ESlateVisibility::Visible);
	if (CountdownWidget) CountdownWidget->SetVisibility(ESlateVisibility::Hidden);
	if (ResultWidget)    ResultWidget->SetVisibility(ESlateVisibility::Visible);
	if (ScoreboardWidget) ScoreboardWidget->SetVisibility(ESlateVisibility::Hidden);
}

void ULB_RaidHUDWidget::ShowScoreboard() const
{
	if (BattleHUD) BattleHUD->SetVisibility(ESlateVisibility::Hidden);
	if (CountdownWidget) CountdownWidget->SetVisibility(ESlateVisibility::Hidden);
	if (ResultWidget) ResultWidget->SetVisibility(ESlateVisibility::Hidden);
	if (ScoreboardWidget) ScoreboardWidget->SetVisibility(ESlateVisibility::Visible);
}