// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/CharacterSelect/LB_CharacterSelectWidget.h"

#include "EngineUtils.h"
#include "Components/WrapBox.h"
#include "UI/CharacterSelect/LB_CharacterCardWidget.h"
#include "Engine/DataTable.h"
#include "Engine/TargetPoint.h"
#include "GameState/LB_CharacterSelectGameState.h"
#include "Player/LB_MainMenuPlayerController.h"
#include "Components/HorizontalBox.h"
#include "UI/CharacterSelect/LB_CharacterPreview.h"
#include "UI/CharacterSelect/LB_CharacterSelectSlotWidget.h"

void ULB_CharacterSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildCharacterCards();
	InitLocalCharacterPreview();
	
	ALB_MainMenuPlayerController* PC = Cast<ALB_MainMenuPlayerController>(CachedPlayerController.Get());

	if (!IsValid(PC))
	{
		TryBindGameState();
		return;
	}

	PC->OnCharacterSelectResult.AddUniqueDynamic(
		this, &ThisClass::HandleCharacterSelectResult);
	
	TryBindGameState();
}

void ULB_CharacterSelectWidget::NativeDestruct()
{
	// 재시도 타이머 정리
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(GameStateBindRetryHandle);
	}
	
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
	
	if (ALB_MainMenuPlayerController* PC =
	Cast<ALB_MainMenuPlayerController>(CachedPlayerController.Get()))
	{
		PC->OnCharacterSelectResult.RemoveDynamic(
			this,
			&ThisClass::HandleCharacterSelectResult);
	}
	
	if (UWorld* World = GetWorld())
	{
		if (ALB_CharacterSelectGameState* GS =
			World->GetGameState<ALB_CharacterSelectGameState>())
		{
			GS->OnSnapshotChanged.RemoveDynamic(
				this, &ThisClass::HandleSnapshotChanged);
		}
	}
	
	LastRevision = INDEX_NONE;
	
	PartySlots.Empty();
	
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
		FString FullEnumName = FString::Printf(TEXT("ELBCharacterID::%s"), *RowName.ToString());
		int64 EnumValue = CharEnum->GetValueByName(FName(*FullEnumName));
		
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
	
	// 첫 번째 카드 자동 선택
	if (AllCards.Num() > 0 && IsValid(AllCards[0]))
	{
		OnCharacterCardClicked(AllCards[0]->GetCharacterID());
	}
}

void ULB_CharacterSelectWidget::HandleCharacterSelectResult(ELBCharacterSelectResult Result)
{
	if (Result == ELBCharacterSelectResult::Success)
	{
		BP_OnCharacterConfirmed();
		return;
	}

	BP_OnCharacterSelectFailed(Result);
}

void ULB_CharacterSelectWidget::HandleSnapshotChanged(const FLBCharacterSelectSnapshot& Snapshot)
{
	if (LastRevision != INDEX_NONE && Snapshot.Revision <= LastRevision)
	{
		return;
	}

	LastRevision = Snapshot.Revision;
	
	ApplySnapshot(Snapshot);
}

void ULB_CharacterSelectWidget::ApplySnapshot(const FLBCharacterSelectSnapshot& Snapshot)
{
	UpdatePartySlots(Snapshot);
	
	BP_OnSnapshotUpdated(Snapshot);
	
	if (CurrentPhase != Snapshot.Phase)
	{
		CurrentPhase = Snapshot.Phase;

		switch (CurrentPhase)
		{
		case ELBCharacterSelectPhase::Waiting:
			break;

		case ELBCharacterSelectPhase::AllReady:
			BP_OnEveryoneReady();
			break;

		case ELBCharacterSelectPhase::Traveling:
			break;
		}
	}
}

void ULB_CharacterSelectWidget::UpdatePartySlots(
	const FLBCharacterSelectSnapshot& Snapshot)
{
	if (!HB_PartyStatus ||
		!PartySlotWidgetClass ||
		!CharacterDataTable)
	{
		return;
	}

	const int32 PlayerCount = Snapshot.Players.Num();

	// 부족한 슬롯만 생성 (최초 1회 또는 플레이어 증가 시)
	while (PartySlots.Num() < PlayerCount)
	{
		ULB_CharacterSelectSlotWidget* NewSlot =
			CreateWidget<ULB_CharacterSelectSlotWidget>(
				GetOwningPlayer(),
				PartySlotWidgetClass);

		if (!NewSlot)
		{
			break;
		}

		PartySlots.Add(NewSlot);
		HB_PartyStatus->AddChild(NewSlot);
	}

	// 플레이어 수에 맞게 슬롯 표시/숨김
	for (int32 i = 0; i < PartySlots.Num(); ++i)
	{
		if (PartySlots[i])
		{
			PartySlots[i]->SetVisibility(
				i < PlayerCount
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
		}
	}

	// 슬롯 내용만 갱신
	for (int32 i = 0; i < PlayerCount; ++i)
	{
		if (!PartySlots.IsValidIndex(i) || !PartySlots[i])
		{
			continue;
		}

		const FLBCharacterSelectPlayerInfo& PlayerInfo = Snapshot.Players[i];

		const FLBCharacterData* CharacterData = nullptr;

		if (PlayerInfo.CharacterID != ELBCharacterID::None)
		{
			CharacterData = FindCharacterData(PlayerInfo.CharacterID);
		}

		PartySlots[i]->UpdateSlot(PlayerInfo, CharacterData);
	}
}

const FLBCharacterData* ULB_CharacterSelectWidget::FindCharacterData(ELBCharacterID CharacterID) const
{
	if (!CharacterDataTable)
	{
		return nullptr;
	}

	const UEnum* Enum = StaticEnum<ELBCharacterID>();

	if (!Enum)
	{
		return nullptr;
	}

	const FName RowName(
		*Enum->GetNameStringByValue((int64)CharacterID));

	return CharacterDataTable->FindRow<FLBCharacterData>(
		RowName,
		TEXT("FindCharacterData"));
}

void ULB_CharacterSelectWidget::OnCharacterCardClicked(ELBCharacterID ClickedID)
{
	//UE_LOG(LogTemp, Log, TEXT("[SelectWidget] OnCharacterCardClicked: %d"), static_cast<int32>(ClickedID));
	
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
	
	if (CachedPlayerController.IsValid())
	{
		if (ALB_MainMenuPlayerController* PC =
			Cast<ALB_MainMenuPlayerController>(
				CachedPlayerController.Get()))
		{
			PC->Server_SelectCharacterPreview(ClickedID);
		}
	}
	
	if (!CharacterDataTable)
	{
		return;
	}
	
	// ELBCharacterID -> DT_CharacterData의 RowName (FName) 변환 과정
	// [1] ELBCharacterID Enum 정보를 가져옴
	const UEnum* CharacterEnum = StaticEnum<ELBCharacterID>();

	if (!CharacterEnum)
	{
		return;
	}
	
	const FName RowName(*CharacterEnum->GetNameStringByValue(static_cast<int64>(ClickedID)));

	const FLBCharacterData* Data = CharacterDataTable->FindRow<FLBCharacterData>(RowName,TEXT("CharacterSelected"));

	if (!Data)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SelectWidget] Data 없음 RowName: %s"), *RowName.ToString());
		return;
	}

	BP_OnCharacterSelected(SelectedCharacterID, *Data);
}

void ULB_CharacterSelectWidget::OnDetailViewClicked()
{
	if (SelectedCharacterID == ELBCharacterID::None) return;
	
	BP_OnDetailViewRequested();
}

void ULB_CharacterSelectWidget::OnConfirmCharacterClicked()
{
	if (SelectedCharacterID == ELBCharacterID::None) return;
	
	// 선택한 캐릭터를 PlayerState에 저장
	if (CachedPlayerController.IsValid())
	{
		ALB_MainMenuPlayerController* PC = Cast<ALB_MainMenuPlayerController>(CachedPlayerController.Get());

		if (!IsValid(PC))
		{
			return;
		}

		PC->SelectCharacterAndReady(SelectedCharacterID);
	}
	
	// 모두 캐릭터를 셀렉하면 사냥 시작
}

void ULB_CharacterSelectWidget::OnBackToBasicClicked()
{
	BP_OnBasicViewRequested();
}

void ULB_CharacterSelectWidget::InitLocalCharacterPreview()
{
	if (!CharacterDataTable || !CharacterPreviewClass) return;

	LocalCharacterPreviews.Empty();

	TArray<TObjectPtr<ATargetPoint>> TargetPoints;

	for (TActorIterator<ATargetPoint> It(GetWorld()); It; ++It)
	{
		TargetPoints.Add(*It);
	}
	
	TargetPoints.Sort([](const ATargetPoint& A, const ATargetPoint& B)
	{
		return A.GetName() < B.GetName();
	});
	
	TArray<FName> RowNames = CharacterDataTable->GetRowNames();

	RowNames.Sort([](const FName& A, const FName& B)
	{
		return A.LexicalLess(B);
	});
	
	for(int32 Index = 0;
		Index < RowNames.Num();
		Index++)
	{
		const FLBCharacterData* Data =
			CharacterDataTable->FindRow<FLBCharacterData>(
				RowNames[Index],
				TEXT("LocalPreview"));


		if(!Data)
		{
			continue;
		}
		
		if(!TargetPoints.IsValidIndex(Index))
		{
			continue;
		}
		
		FActorSpawnParameters Params;
		
		Params.Owner = GetOwningPlayer();
		
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		
		ALB_CharacterPreview* Preview =
			GetWorld()->SpawnActor<ALB_CharacterPreview>(
				CharacterPreviewClass,
				TargetPoints[Index]->GetActorTransform(),
				Params);
		
		if(!Preview)
		{
			continue;
		}
		
		Preview->Initialize(*Data);
		LocalCharacterPreviews.Add(Preview);
	}
}

void ULB_CharacterSelectWidget::TryBindGameState()
{
	ALB_CharacterSelectGameState* GS =
		GetWorld()->GetGameState<ALB_CharacterSelectGameState>();

	if (!IsValid(GS))
	{
		// GameState 아직 복제 안 됨 → 0.1초 후 재시도
		GetWorld()->GetTimerManager().SetTimer(
			GameStateBindRetryHandle,
			this,
			&ThisClass::TryBindGameState,
			0.1f,
			false);
		return;
	}

	// AddUniqueDynamic → 이미 바인딩돼 있으면 중복 등록 안 함
	GS->OnSnapshotChanged.AddUniqueDynamic(
		this, &ThisClass::HandleSnapshotChanged);

	// 현재 스냅샷으로 즉시 갱신
	HandleSnapshotChanged(GS->GetSnapshot());
}