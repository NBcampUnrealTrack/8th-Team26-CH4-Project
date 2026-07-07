// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Panels/LB_TimerWidget.h"

void ULB_TimerWidget::SetRemainingTime(float RemainingTime)
{
	const int32 TotalSeconds = FMath::Max( 0, FMath::CeilToInt(RemainingTime));
	const int32 Minutes = FMath::FloorToInt(RemainingTime / 60.f);
	const int32 Seconds = FMath::FloorToInt(FMath::Fmod(RemainingTime, 60.f));

	BP_SetRemainingTime(Minutes, Seconds);
}
