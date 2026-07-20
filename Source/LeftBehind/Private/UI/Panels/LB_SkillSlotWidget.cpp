// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Panels/LB_SkillSlotWidget.h"
#include "Components/Image.h"
#include "System/Character/LBCharacterTypes.h"

void ULB_SkillSlotWidget::SetSkillInfo(const FLBSkillInfo& InSkillInfo)
{
	if (IMG_InputKey && InSkillInfo.InputKeyImage)
		IMG_InputKey->SetBrushFromTexture(InSkillInfo.InputKeyImage);

	if (IMG_SkillIcon && InSkillInfo.SkillIcon)
		IMG_SkillIcon->SetBrushFromTexture(InSkillInfo.SkillIcon);
}
