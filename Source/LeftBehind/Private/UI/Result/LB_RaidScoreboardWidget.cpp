// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Result/LB_RaidScoreboardWidget.h"
#include "Components/HorizontalBox.h"
#include "GameFramework/PlayerController.h"
#include "System/Raid/LBRaidTypes.h"
#include "UI/Result/LB_RaidScoreSlotWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogLBRaidScoreboard, Log, All);

ULB_RaidScoreboardWidget::ULB_RaidScoreboardWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RaidScoreSlotClass = TSoftClassPtr<ULB_RaidScoreSlotWidget>(FSoftObjectPath(
		TEXT("/Game/LeftBehind/UI/BattleHUD/Result/WBP_LB_RaidScoreSlotWidget.WBP_LB_RaidScoreSlotWidget_C")));
}

void ULB_RaidScoreboardWidget::HandleRaidScoreboardChanged(const FLBRaidScoreboardData& ScoreboardData)
{
	Super::HandleRaidScoreboardChanged(ScoreboardData);
	
	CachedScoreboardData = ScoreboardData;
	
	RefreshScoreboard();
}

void ULB_RaidScoreboardWidget::RefreshScoreboard()
{
	if (!IsValid(PlayerResultContainer))
	{
		return;
	}

	APlayerController* OwningPlayer = GetOwningPlayer();
	if (!IsValid(OwningPlayer))
	{
		UE_LOG(LogLBRaidScoreboard, Error, TEXT("Cannot build the raid scoreboard without an owning player."));
		return;
	}

	UClass* LoadedRaidScoreSlotClass = RaidScoreSlotClass.LoadSynchronous();
	if (!IsValid(LoadedRaidScoreSlotClass)
		|| !LoadedRaidScoreSlotClass->IsChildOf(ULB_RaidScoreSlotWidget::StaticClass()))
	{
		UE_LOG(
			LogLBRaidScoreboard,
			Error,
			TEXT("Raid score slot widget class is unavailable. Path=%s"),
			*RaidScoreSlotClass.ToSoftObjectPath().ToString());
		return;
	}
	
	PlayerResultContainer->ClearChildren();
	
	for (const FLBPlayerFinalResult& PlayerResult : CachedScoreboardData.PlayerResults)
	{
		ULB_RaidScoreSlotWidget* ResultSlot = 
			CreateWidget<ULB_RaidScoreSlotWidget>(OwningPlayer, LoadedRaidScoreSlotClass);
		
		if (!ResultSlot) continue;
		
		ResultSlot->SetPlayerResult(PlayerResult);
		
		PlayerResultContainer->AddChild(ResultSlot);
	}
}
