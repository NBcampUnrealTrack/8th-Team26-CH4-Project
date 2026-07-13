// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/CharacterSelect/LB_CharacterCardWidget.h"
#include "Components/Button.h"

void ULB_CharacterCardWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (BTN_CardButton)
	{
		BTN_CardButton->OnClicked.AddDynamic(
			this, &ThisClass::HandleCardButtonClicked);
	}
}

void ULB_CharacterCardWidget::SetCharacterData(ELBCharacterID InCharacterID, const FLBCharacterData& InData)
{
	CharacterID = InCharacterID;
	BP_OnDataSet(InCharacterID, InData);
}

void ULB_CharacterCardWidget::SetSelected(bool bInSelected)
{
	bIsSelected = bInSelected;
	BP_OnSelectionChanged(bIsSelected);
}

void ULB_CharacterCardWidget::HandleCardButtonClicked()
{
	OnCardClicked.Broadcast(CharacterID);
}
