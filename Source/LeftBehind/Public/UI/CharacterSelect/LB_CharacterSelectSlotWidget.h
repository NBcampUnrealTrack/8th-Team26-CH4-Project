// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Base/LB_BaseUserWidget.h"
#include "LB_CharacterSelectSlotWidget.generated.h"

class UImage;
class UTextBlock;
struct FLBCharacterSelectPlayerInfo;
struct FLBCharacterData;

UCLASS()
class LEFTBEHIND_API ULB_CharacterSelectSlotWidget : public ULB_BaseUserWidget
{
	GENERATED_BODY()
public:

	void UpdateSlot(
		const FLBCharacterSelectPlayerInfo& PlayerInfo,
		const FLBCharacterData* CharacterData);

protected:

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> TXT_PlayerName;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UImage> IMG_Portrait;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> TXT_ReadyStatus;

};
