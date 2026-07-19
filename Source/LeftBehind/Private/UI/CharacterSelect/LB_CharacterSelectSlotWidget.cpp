// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/CharacterSelect/LB_CharacterSelectSlotWidget.h"
#include "System/Character/LBCharacterTypes.h"
#include "System/CharacterSelect/LBCharacterSelectTypes.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void ULB_CharacterSelectSlotWidget::UpdateSlot(
	const FLBCharacterSelectPlayerInfo& PlayerInfo,
	const FLBCharacterData* CharacterData)
{
	if (TXT_PlayerName)
	{
		TXT_PlayerName->SetText(FText::FromString(PlayerInfo.PlayerName));
	}

	// 캐릭터 선택 여부
	if (!CharacterData)
	{
		if (TXT_ReadyStatus)
		{
			TXT_ReadyStatus->SetText(FText::FromString(TEXT("대기 중")));
		}
	}
	else
	{
		BP_OnUpdateSlot(*CharacterData);
		
		if (PlayerInfo.bReady)
		{
			if (TXT_ReadyStatus) TXT_ReadyStatus->SetText(FText::FromString(TEXT("선택 완료")));
		}
		else
		{
			if (TXT_ReadyStatus) TXT_ReadyStatus->SetText(FText::FromString(TEXT("선택 중")));
		}
	}
}