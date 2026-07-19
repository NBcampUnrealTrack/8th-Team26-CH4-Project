// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "System/Character/LBCharacterTypes.h"
#include "UI/Base/LB_BaseUserWidget.h"
#include "System/CharacterSelect/LBCharacterSelectTypes.h"
#include "LB_CharacterSelectWidget.generated.h"

// 캐릭터 선택창 루트 위젯
// DT_CharacterData를 읽어 캐릭터 카드 동적 생성
// 캐릭터 선택/확정 처리

class ATargetPoint;
class UWrapBox;
class ULB_CharacterCardWidget;
class UDataTable;
class ALB_CharacterSelectGameState;
class UVerticalBox;
class ULB_CharacterSelectSlotWidget;
class ALB_CharacterPreview;

UCLASS()
class LEFTBEHIND_API ULB_CharacterSelectWidget : public ULB_BaseUserWidget
{
	GENERATED_BODY()
	
protected:
	
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	
	// DT_CharacterData를 읽어 역할별로 카드를 생성
	void BuildCharacterCards();
	
	UFUNCTION()
	void HandleCharacterSelectResult(ELBCharacterSelectResult Result);
	
	UFUNCTION()
	void HandleSnapshotChanged(const FLBCharacterSelectSnapshot& Snapshot);
	
	UFUNCTION()
	void ApplySnapshot(const FLBCharacterSelectSnapshot& Snapshot);
	
	UFUNCTION()
	void UpdatePartySlots(const FLBCharacterSelectSnapshot& Snapshot);
	
	const FLBCharacterData* FindCharacterData(ELBCharacterID CharacterID) const;
	
	// 버튼 이벤트 ------------------------------------
	
	// 캐릭터 카드 클릭
	UFUNCTION()
	void OnCharacterCardClicked(ELBCharacterID ClickedID);

	// 캐릭터 확정
	UFUNCTION(BlueprintCallable, Category="LB|CharacterSelect")
	void OnConfirmCharacterClicked();

	// BP 확장지점 ----------------------------------
	
	// 카드 생성 완료 후, 카드 등장 애니메이션
	UFUNCTION(BlueprintImplementableEvent, Category="LB|CharacterSelect")
	void BP_OnCardsBuilt();
	
	// 3D 프리뷰 갱신, 상세정보 버튼 활성화
	UFUNCTION(BlueprintImplementableEvent, Category="LB|CharacterSelect")
	void BP_OnCharacterSelected(ELBCharacterID SelectedID, const FLBCharacterData& Data);
	
	UFUNCTION(BlueprintImplementableEvent, Category="LB|CharacterSelect")
	void BP_OnCharacterConfirmed();
	
	UFUNCTION(BlueprintImplementableEvent, Category="LB|CharacterSelect")
	void BP_OnCharacterSelectFailed(ELBCharacterSelectResult Result);
	
	UFUNCTION(BlueprintImplementableEvent, Category="LB|CharacterSelect")
	void BP_OnSnapshotUpdated(const FLBCharacterSelectSnapshot& Snapshot);
	
	UFUNCTION(BlueprintImplementableEvent, Category="LB|CharacterSelect")
	void BP_OnEveryoneReady();
	
	// 바인드 위젯 ----------------------------------------
	
	// 딜러 카드가 들어갈 컨테이너
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWrapBox> DPSCardContainer;
	
	// 힐러 카드가 들어갈 컨테이너
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWrapBox> HealerCardContainer;
	
	// 파티원 캐릭터 선택 현황 컨테이너
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UVerticalBox> VB_PartyStatus;
	
	// Class Defaults 지정 --------------------------------
	
	// 캐릭터 카드 BP 클래스
	UPROPERTY(EditDefaultsOnly, Category="LB|CharacterSelect")
	TSubclassOf<ULB_CharacterCardWidget> CharacterCardClass;
	
	// 파티원 슬롯 BP 클래스
	UPROPERTY(EditDefaultsOnly, Category="LB|CharacterSelect")
	TSubclassOf<ULB_CharacterSelectSlotWidget> PartySlotWidgetClass;
	
	// 캐릭터 데이터 테이블
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="LB|CharacterSelect")
	TObjectPtr<UDataTable> CharacterDataTable;
	
	
	UPROPERTY(BlueprintReadOnly, Category="LB|CharacterSelect")
	TArray<TObjectPtr<ULB_CharacterCardWidget>> AllCards;
	
	UPROPERTY(BlueprintReadOnly, Category="LB|CharacterSelect")
	ELBCharacterID SelectedCharacterID = ELBCharacterID::None;
	
	UPROPERTY()
	TArray<TObjectPtr<ULB_CharacterSelectSlotWidget>> PartySlots;
	
	UPROPERTY(EditDefaultsOnly, Category="LB|CharacterSelect")
	TSubclassOf<ALB_CharacterPreview> CharacterPreviewClass;
	
private:
	
	void InitLocalCharacterPreview();

	UPROPERTY()
	TArray<TObjectPtr<ALB_CharacterPreview>> LocalCharacterPreviews;
	
	UPROPERTY()
	TObjectPtr<ULB_CharacterCardWidget> PreviousSelectedCard = nullptr;
	
	ELBCharacterSelectPhase CurrentPhase = ELBCharacterSelectPhase::Waiting;
	
	int32 LastRevision = INDEX_NONE;
	
	FTimerHandle GameStateBindRetryHandle;
	
	UFUNCTION()
	void TryBindGameState();
};
