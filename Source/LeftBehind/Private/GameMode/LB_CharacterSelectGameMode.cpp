// Fill out your copyright notice in the Description page of Project Settings.


#include "GameMode/LB_CharacterSelectGameMode.h"
#include "Engine/DataTable.h"
#include "System/Raid/LBCharacterTypes.h"
#include "UI/CharacterSelect/LB_CharacterPreview.h"
#include "UI/CharacterSelect/LB_CharacterSelectWidget.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"

void ALB_CharacterSelectGameMode::BeginPlay()
{
	Super::BeginPlay();
	
	SetupInput();
	GetTargetPoints();
	InitCharacterPreview();
	CreateCharacterSelectWidget();
}

void ALB_CharacterSelectGameMode::SetupInput()
{
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();

	if (!PlayerController)
	{
		return;
	}

	PlayerController->bShowMouseCursor = true;

	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

	PlayerController->SetInputMode(InputMode);
}

void ALB_CharacterSelectGameMode::GetTargetPoints()
{
	TargetPoints.Empty();

	for (TActorIterator<ATargetPoint> It(GetWorld()); It; ++It)
	{
		TargetPoints.Add(*It);
	}

	TargetPoints.Sort([](const TObjectPtr<ATargetPoint>& A,
						 const TObjectPtr<ATargetPoint>& B)
	{
		return A->GetName() < B->GetName();
	});
}

void ALB_CharacterSelectGameMode::InitCharacterPreview()
{
	if (!CharacterDataTable || !CharacterPreviewClass) return;

	CharacterPreviews.Empty();

	TArray<FName> RowNames;
	GetCharacterRows(RowNames);

	for (int32 Index = 0; Index < RowNames.Num(); Index++)
	{
		const FLBCharacterData* Data = CharacterDataTable->FindRow<FLBCharacterData>(
			RowNames[Index], TEXT("CharacterPreview"));
		if (!Data) continue;

		// TargetPoint 없으면 경고 후 스킵
		if (!TargetPoints.IsValidIndex(Index))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[CharacterSelectGameMode] TargetPoint 부족. Index: %d"), Index);
			continue;
		}

		FTransform SpawnTransform = TargetPoints[Index]->GetActorTransform();

		FActorSpawnParameters SpawnParams;
		ALB_CharacterPreview* Preview = GetWorld()->SpawnActor<ALB_CharacterPreview>(
			CharacterPreviewClass,
			SpawnTransform,   // ← TargetPoint 위치에 스폰
			SpawnParams);

		if (!Preview) continue;

		Preview->Initialize(*Data);
		CharacterPreviews.Add(Preview);
	}
}

void ALB_CharacterSelectGameMode::GetCharacterRows(TArray<FName>& OutRows) const
{
	OutRows.Empty();

	if (!CharacterDataTable)
	{
		return;
	}

	OutRows = CharacterDataTable->GetRowNames();

	OutRows.Sort([](const FName& A, const FName& B)
	{
		return A.LexicalLess(B);
	});
}

void ALB_CharacterSelectGameMode::CreateCharacterSelectWidget()
{
	if (!CharacterSelectWidgetClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[CharacterSelectGameMode] CharacterSelectWidgetClass 없음"));
		return;
	}

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!IsValid(PC))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[CharacterSelectGameMode] PlayerController 없음"));
		return;
	}

	ULB_CharacterSelectWidget* Widget =
		CreateWidget<ULB_CharacterSelectWidget>(PC, CharacterSelectWidgetClass);

	if (!IsValid(Widget))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[CharacterSelectGameMode] Widget 생성 실패"));
		return;
	}

	Widget->AddToViewport(10);
}
