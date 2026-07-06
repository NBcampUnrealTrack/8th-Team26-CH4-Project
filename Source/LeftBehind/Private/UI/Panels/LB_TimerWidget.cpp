// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Panels/LB_TimerWidget.h"

void ULB_TimerWidget::SetRemainingTime(float RemainingTime)
{
	const int32 TotalSeconds = FMath::Max( 0, FMath::CeilToInt(RemainingTime));
	const int32 Minutes = TotalSeconds / 60;
	const int32 Seconds = TotalSeconds % 60;

	BP_SetRemainingTime(Minutes, Seconds);
}
