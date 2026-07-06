// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Popup/LB_CountdownWidget.h"
#include "GameState/LB_RaidGameState.h"
#include "TimerManager.h"

void ULB_CountdownWidget::NativeDestruct()
{
	StopCountdownTimer();
	Super::NativeDestruct();
}

void ULB_CountdownWidget::BP_OnRaidStateChanged_Implementation(ELBRaidState NewState)
{
	Super::BP_OnRaidStateChanged_Implementation(NewState);
	
	if (NewState == ELBRaidState::Countdown)
	{
		// 무조건 최초 1회 숫자 갱신되도록 INDEX_NONE 넣기
		CachedSecond = INDEX_NONE;
		StartCountdownTimer();
	}
	else
	{
		StopCountdownTimer();
	}
}

void ULB_CountdownWidget::StartCountdownTimer()
{
	if (!GetWorld()) return;

	GetWorld()->GetTimerManager().SetTimer(
		CountdownTimerHandle,
		this,
		&ThisClass::UpdateCountdown,
		0.1f,
		true);

	UpdateCountdown();
}

void ULB_CountdownWidget::StopCountdownTimer()
{
	if (!GetWorld()) return;
	
	GetWorld()->GetTimerManager().ClearTimer(CountdownTimerHandle);
}

void ULB_CountdownWidget::UpdateCountdown()
{
	if (!CachedRaidGameState) return;
	
	const int32 RemainingSecond = FMath::CeilToInt(CachedRaidGameState->GetCountdownRemaining());
	
	// 숫자가 바뀔 때만 갱신
	if (RemainingSecond == CachedSecond) return;
	
	CachedSecond = RemainingSecond;

	BP_UpdateCountdown(RemainingSecond);
}
