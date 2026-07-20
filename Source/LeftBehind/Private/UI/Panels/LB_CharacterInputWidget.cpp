// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Panels/LB_CharacterInputWidget.h"
#include "System/Character/LBCharacterTypes.h"
#include "Components/HorizontalBox.h"
#include "Player/LB_PlayerState.h"
#include "UI/Panels/LB_SkillSlotWidget.h"

void ULB_CharacterInputWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshSkills();
}

void ULB_CharacterInputWidget::RefreshSkills()
{
	if (!CharacterDataTable || !HB_Root || !SkillSlotWidgetClass) return;

	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;

	ALB_PlayerState* PS = PC->GetPlayerState<ALB_PlayerState>();
	if (!PS) return;

	const FString RowName =
		StaticEnum<ELBCharacterID>()
		->GetNameStringByValue((int64)PS->GetCharacterID());

	const FLBCharacterData* CharacterData =
		CharacterDataTable->FindRow<FLBCharacterData>(
			FName(*RowName),
			TEXT("CharacterInput"));

	if (!CharacterData) return;

	HB_Root->ClearChildren();
	SkillSlots.Empty();

	for (const FLBSkillInfo& Skill : CharacterData->Skills)
	{
		ULB_SkillSlotWidget* SkillSlot = CreateWidget<ULB_SkillSlotWidget>(
			GetOwningPlayer(), SkillSlotWidgetClass);
		if (!Slot) continue;

		SkillSlot->SetSkillInfo(Skill);
		HB_Root->AddChild(SkillSlot);
		SkillSlots.Add(SkillSlot);
	}
}
