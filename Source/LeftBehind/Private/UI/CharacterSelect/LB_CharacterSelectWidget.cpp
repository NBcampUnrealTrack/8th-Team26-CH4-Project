// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/CharacterSelect/LB_CharacterSelectWidget.h"

#include "Components/WrapBox.h"
#include "UI/CharacterSelect/LB_CharacterCardWidget.h"
#include "Engine/DataTable.h"
#include "Player/LB_PlayerState.h"

void ULB_CharacterSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildCharacterCards();
}

void ULB_CharacterSelectWidget::NativeDestruct()
{
	// 각 캐릭터 카드마다 클릭이벤트를 구독하고 있기 때문에 for문으로 바인딩 해제
	
	for (ULB_CharacterCardWidget* Card : AllCards)
	{
		if (IsValid(Card))
		{
			// 잠금 카드는 바인딩 안 되어 있음
			// RemoveDynamic은 바인딩되지 않은 델리게이트에 호출해도 에러 없이 무시됨
			Card->OnCardClicked.RemoveDynamic(this, &ThisClass::OnCharacterCardClicked);
		}
	}
	
	Super::NativeDestruct();
}

void ULB_CharacterSelectWidget::BuildCharacterCards()
{
	if (!CharacterCardClass || !CharacterDataTable) return;
	
	if (DPSCardContainer) DPSCardContainer->ClearChildren();
	if (HealerCardContainer) HealerCardContainer->ClearChildren();
	AllCards.Empty();
	
	// DT_CharacterData의 RowName (FName) → ELBCharacterID 변환 과정
	// [1] ELBCharacterID Enum 정보를 가져옴
	const UEnum* CharEnum = StaticEnum<ELBCharacterID>();
	if (!CharEnum) return;
	
	TArray<FName> RowNames = CharacterDataTable->GetRowNames();
	for (const FName& RowName : RowNames)
	{
		FLBCharacterData* Data = CharacterDataTable->FindRow<FLBCharacterData>(RowName, TEXT("BuildCharacterCards"));
		if (!Data) continue;
		
		// [2] Row Name으로 숫자값 조회
		int64 EnumValue = CharEnum->GetValueByName(RowName);
		
		// 매칭되는 Enum이 없으면 INDEX_NONE(-1) 반환
		if (EnumValue == INDEX_NONE) continue;
		
		// [3] 숫자(int64) → ELBCharacterID로 변환
		ELBCharacterID CharacterID = static_cast<ELBCharacterID>(EnumValue);
		
		// 카드 생성 -> 역할별 컨테이너와 AllCards에 넣어주기
		ULB_CharacterCardWidget* Card = CreateWidget<ULB_CharacterCardWidget>(CachedPlayerController.Get(), CharacterCardClass);
		if (!Card) continue;
		
		Card->SetCharacterData(CharacterID, *Data);
		
		// 잠금 여부에 따라 클릭 이벤트 바인딩 분기.
		if (!Data->bLocked)
		{
			Card->OnCardClicked.AddDynamic(this, &ThisClass::OnCharacterCardClicked);
		}
		
		AllCards.Add(Card);
		
		if (Data->RoleType == ELBRoleType::DPS)
		{
			if (DPSCardContainer) DPSCardContainer->AddChild(Card);
		}
		else if (Data->RoleType == ELBRoleType::Healer)
		{
			if (HealerCardContainer) HealerCardContainer->AddChild(Card);
		}
	}
	
	BP_OnCardsBuilt();
}

void ULB_CharacterSelectWidget::OnCharacterCardClicked(ELBCharacterID ClickedID)
{
	if (IsValid(PreviousSelectedCard))
	{
		PreviousSelectedCard->SetSelected(false);
	}
	
	for (ULB_CharacterCardWidget* Card : AllCards)
	{
		if (Card->GetCharacterID() == ClickedID)
		{
			Card->SetSelected(true);
			PreviousSelectedCard = Card;
			break;
		}
	}
	
	SelectedCharacterID = ClickedID;
	
	if (!CharacterDataTable) return;
	
	// ELBCharacterID -> DT_CharacterData의 RowName (FName) 변환 과정
	// [1] ELBCharacterID Enum 정보를 가져옴
	const UEnum* CharEnum = StaticEnum<ELBCharacterID>();
	if (!CharEnum) return;
	
	// [2] 숫자값으로 Row Name 조회
	FName RowName = CharEnum->GetNameByValue(static_cast<int64>(ClickedID));
	
	FLBCharacterData* Data = CharacterDataTable->FindRow<FLBCharacterData>(RowName, TEXT("OnCharacterCardClicked"));
	
	if (!Data) return;
	
	BP_OnCharacterSelected(SelectedCharacterID, *Data);
}

void ULB_CharacterSelectWidget::OnDetailViewClicked()
{
	if (SelectedCharacterID == ELBCharacterID::None) return;
	
	BP_OnDetailViewRequested();
}

void ULB_CharacterSelectWidget::OnEnterWaitingRoomClicked()
{
	if (SelectedCharacterID == ELBCharacterID::None) return;
	
	// 선택한 캐릭터를 PlayerState에 저장
	if (CachedPlayerController.IsValid())
	{
		ALB_PlayerState* LBPS = Cast<ALB_PlayerState>(CachedPlayerController->PlayerState);
		if (!IsValid(LBPS)) return;
		
		LBPS->SetCharacterID_ServerOnly(SelectedCharacterID);
	}
	
	// 이후, 대기실 이동
}

void ULB_CharacterSelectWidget::OnBackToBasicClicked()
{
	BP_OnBasicViewRequested();
}
