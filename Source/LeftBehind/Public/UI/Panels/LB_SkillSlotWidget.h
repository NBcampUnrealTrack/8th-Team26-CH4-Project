// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Base/LB_BaseUserWidget.h"
#include "LB_SkillSlotWidget.generated.h"

class UImage;
struct FLBSkillInfo;

UCLASS()
class LEFTBEHIND_API ULB_SkillSlotWidget : public ULB_BaseUserWidget
{
	GENERATED_BODY()
	
public:
	void SetSkillInfo(const FLBSkillInfo& InSkillInfo);

protected:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> IMG_InputKey;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> IMG_SkillIcon;
};
