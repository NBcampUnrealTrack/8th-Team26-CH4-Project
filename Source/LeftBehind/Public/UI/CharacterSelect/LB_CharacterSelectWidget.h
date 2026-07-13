// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "System/Raid/LBCharacterTypes.h"
#include "UI/Base/LB_BaseUserWidget.h"
#include "LB_CharacterSelectWidget.generated.h"

// 캐릭터 선택창 루트 위젯
// DT_CharacterData를 읽어 캐릭터 카드 동적 생성
// 캐릭터 선택/확정 처리

class UWrapBox;
class ULB_CharacterCardWidget;
class UDataTable;

UCLASS()
class LEFTBEHIND_API ULB_CharacterSelectWidget : public ULB_BaseUserWidget
{
	GENERATED_BODY()
	
protected:
	
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	
	// DT_CharacterData를 읽어 역할별로 카드를 생성
	void BuildCharacterCards();
	
	// 버튼 이벤트 3가지 ------------------------------------
	
	// 캐릭터 카드 클릭
	UFUNCTION()
	void OnCharacterCardClicked(ELBCharacterID ClickedID);
	
	// 상세정보 버튼 클릭
	UFUNCTION()
	void OnDetailViewClicked();
	
	// 대기실 입장 버튼 클릭
	UFUNCTION()
	void OnEnterWaitingRoomClicked();
	
	// 상세 화면에서 기본 화면으로 복귀
	UFUNCTION(BlueprintCallable, Category="LB|CharacterSelect")
	void OnBackToBasicClicked();
	
	// BP 확장지점 4가지 ----------------------------------
	
	// 카드 생성 완료 후, 카드 등장 애니메이션
	UFUNCTION(BlueprintImplementableEvent, Category="LB|CharacterSelect")
	void BP_OnCardsBuilt();
	
	// 3D 프리뷰 갱신, 상세정보 버튼 활성화
	UFUNCTION(BlueprintImplementableEvent, Category="LB|CharacterSelect")
	void BP_OnCharacterSelected(ELBCharacterID SelectedID, const FLBCharacterData& Data);

	// 3D 프리뷰 갱신, 대각 와이프 애니메이션, 상세정보 표시
	UFUNCTION(BlueprintImplementableEvent, Category="LB|CharacterSelect")
	void BP_OnDetailViewRequested();

	// 역방향 와이프 애니메이션 후, 기본 화면 복귀
	UFUNCTION(BlueprintImplementableEvent, Category="LB|CharacterSelect")
	void BP_OnBasicViewRequested();

	
	// 바인드 위젯 ----------------------------------------
	
	// 딜러 카드가 들어갈 컨테이너
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWrapBox> DPSCardContainer;
	
	// 힐러 카드가 들어갈 컨테이너
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWrapBox> HealerCardContainer;
	
	// Class Defaults 지정 --------------------------------
	
	// 캐릭터 카드 BP 클래스
	UPROPERTY(EditDefaultsOnly, Category="LB|CharacterSelect")
	TSubclassOf<ULB_CharacterCardWidget> CharacterCardClass;
	
	// 캐릭터 데이터 테이블
	UPROPERTY(EditDefaultsOnly, Category="LB|CharacterSelect")
	TObjectPtr<UDataTable> CharacterDataTable;
	
private:
	UPROPERTY()
	TObjectPtr<ULB_CharacterCardWidget> PreviousSelectedCard = nullptr;
	
	UPROPERTY()
	ELBCharacterID SelectedCharacterID = ELBCharacterID::None;
	
	UPROPERTY()
	TArray<TObjectPtr<ULB_CharacterCardWidget>> AllCards;
};
