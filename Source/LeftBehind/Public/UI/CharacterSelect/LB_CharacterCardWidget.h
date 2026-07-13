// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "System/Raid/LBCharacterTypes.h"
#include "UI/Base/LB_BaseUserWidget.h"
#include "LB_CharacterCardWidget.generated.h"

// 캐릭터 선택창에서 캐릭터 하나를 표시하는 카드 위젯

// BP에서 카드 클릭 시 OnCardClicked 디스패처를 통해 캐릭터ID를 선택창에 알림
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCharacterCardClicked, ELBCharacterID, ClickedCharacterID);

UCLASS()
class LEFTBEHIND_API ULB_CharacterCardWidget : public ULB_BaseUserWidget
{
	GENERATED_BODY()
	
public:

	// 카드에 표시할 캐릭터 데이터를 설정
	UFUNCTION(BlueprintCallable, Category="LB|CharacterSelect")
	void SetCharacterData(ELBCharacterID InCharacterID, const FLBCharacterData& InData);

	// 카드 선택/해제 상태를 갱신
	UFUNCTION(BlueprintCallable, Category="LB|CharacterSelect")
	void SetSelected(bool bInSelected);

	// 카드가 클릭됐을 때 선택창으로 알리는 디스패처
	UPROPERTY(BlueprintAssignable, Category="LB|CharacterSelect")
	FOnCharacterCardClicked OnCardClicked;

	// 이 카드가 나타내는 캐릭터 ID
	UFUNCTION(BlueprintPure, Category="LB|CharacterSelect")
	ELBCharacterID GetCharacterID() const { return CharacterID; }

protected:

	// BP에서 카드 UI 갱신 -- 카드 생성 시 1회만 호출
	UFUNCTION(BlueprintImplementableEvent, Category="LB|CharacterSelect")
	void BP_OnDataSet(ELBCharacterID InCharacterID, const FLBCharacterData& InData);

	// BP에서 선택/해제 시각 처리
	UFUNCTION(BlueprintImplementableEvent, Category="LB|CharacterSelect")
	void BP_OnSelectionChanged(bool bInSelected);

private:

	ELBCharacterID CharacterID = ELBCharacterID::None;
	bool bIsSelected = false;
};
