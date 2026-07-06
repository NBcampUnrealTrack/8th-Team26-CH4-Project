// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "../Base/LB_BaseUserWidget.h"
#include "LB_PartyStatusWidget.generated.h"

// GameState의 Player 배열 -> PlayerState
// 플레이어 숫자만큼 목록 관리

class UVerticalBox;
class ULB_PartyMemberSlotWidget;

UCLASS()
class LEFTBEHIND_API ULB_PartyStatusWidget : public ULB_BaseUserWidget
{
	GENERATED_BODY()
	
protected:

	virtual void NativeConstruct() override;

	// 현재 PlayerArray를 기준으로 파티 슬롯 다시 생성
	void RefreshPartyMembers();

protected:

	// 파티 슬롯을 생성할 부모
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> PartyMemberContainer;

	// 파티원 슬롯 BP
	UPROPERTY(EditDefaultsOnly, Category="LB|Party")
	TSubclassOf<ULB_PartyMemberSlotWidget> PartyMemberSlotClass;
};
