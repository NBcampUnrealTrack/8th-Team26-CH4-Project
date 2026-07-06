// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Panels/LB_BossHPWidget.h"

void ULB_BossHPWidget::SetBossHP(float CurrentHP, float MaxHP)
{
	const float Ratio = (MaxHP > 0.f) ? CurrentHP / MaxHP : 0.f;

	BP_SetBossHP(CurrentHP, MaxHP, Ratio);
}
