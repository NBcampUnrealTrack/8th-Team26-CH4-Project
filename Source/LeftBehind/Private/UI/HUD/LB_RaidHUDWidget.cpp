// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/HUD/LB_RaidHUDWidget.h"
#include "UI/HUD/LB_BattleHUDWidget.h"
#include "UI/Popup/LB_CountdownWidget.h"
#include "UI/Popup/LB_RaidResultWidget.h"
#include "Components/Overlay.h"

void ULB_RaidHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ShowWaiting();
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

void ULB_RaidHUDWidget::ShowWaiting()
{
	if (BattleHUD)
	{
		BattleHUD->SetVisibility(ESlateVisibility::Hidden);
	}

	if (CountdownWidget)
	{
		CountdownWidget->SetVisibility(ESlateVisibility::Hidden);
	}

	if (ResultOverlay)
	{
		ResultOverlay->SetVisibility(ESlateVisibility::Hidden);
	}
}

void ULB_RaidHUDWidget::ShowCountdown()
{
	if (BattleHUD)
	{
		BattleHUD->SetVisibility(ESlateVisibility::Hidden);
	}

	if (CountdownWidget)
	{
		CountdownWidget->SetVisibility(ESlateVisibility::Visible);
	}

	if (ResultOverlay)
	{
		ResultOverlay->SetVisibility(ESlateVisibility::Hidden);
	}
}

void ULB_RaidHUDWidget::ShowBattle()
{
	if (BattleHUD)
	{
		BattleHUD->SetVisibility(ESlateVisibility::Visible);
	}

	if (CountdownWidget)
	{
		CountdownWidget->SetVisibility(ESlateVisibility::Hidden);
	}

	if (ResultOverlay)
	{
		ResultOverlay->SetVisibility(ESlateVisibility::Hidden);
	}
}

void ULB_RaidHUDWidget::ShowResult()
{
	if (BattleHUD)
	{
		BattleHUD->SetVisibility(ESlateVisibility::Visible);
	}

	if (CountdownWidget)
	{
		CountdownWidget->SetVisibility(ESlateVisibility::Hidden);
	}

	if (ResultOverlay)
	{
		ResultOverlay->SetVisibility(ESlateVisibility::Visible);
	}
}
