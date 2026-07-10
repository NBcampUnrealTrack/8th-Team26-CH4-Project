#include "UI/MainMenu/LB_MainMenuWaitingWidget.h"

#include "GameFramework/PlayerState.h"
#include "GameState/LB_MainMenuGameState.h"
#include "Player/LB_MainMenuPlayerController.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "LBMainMenuWaitingWidget"

TSharedRef<SWidget> ULB_MainMenuWaitingWidget::RebuildWidget()
{
	const FSlateFontInfo HeadingFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 28);
	const FSlateFontInfo BodyFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 18);

	return SNew(SBorder)
		.Padding(FMargin(36.f))
		.BorderBackgroundColor(FLinearColor(0.015f, 0.02f, 0.025f, 0.94f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 18.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Title", "HUNT LOBBY"))
				.Font(HeadingFont)
				.ColorAndOpacity(FLinearColor(0.86f, 0.91f, 0.95f))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f)
			[
				SAssignNew(PlayerCountText, STextBlock).Font(BodyFont)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f)
			[
				SAssignNew(ConfirmedCountText, STextBlock).Font(BodyFont)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f)
			[
				SAssignNew(TargetMapText, STextBlock).Font(BodyFont)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f, 0.f, 12.f)
			[
				SAssignNew(PhaseText, STextBlock).Font(BodyFont)
			]
			+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 8.f)
			[
				SNew(SBorder)
				.Padding(FMargin(14.f))
				.BorderBackgroundColor(FLinearColor(0.04f, 0.05f, 0.06f, 0.95f))
				[
					SAssignNew(PlayerListBox, SVerticalBox)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 18.f, 0.f, 0.f)
			[
				SAssignNew(StartButton, SButton)
				.Text(LOCTEXT("Start", "START RAID"))
				.ContentPadding(FMargin(32.f, 12.f))
				.OnClicked_UObject(this, &ThisClass::HandleStartClicked)
			]
		];
}

void ULB_MainMenuWaitingWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindGameState();
}

void ULB_MainMenuWaitingWidget::NativeDestruct()
{
	if (IsValid(BoundGameState))
	{
		BoundGameState->OnMainMenuSnapshotChanged.RemoveDynamic(this, &ThisClass::HandleSnapshotChanged);
	}
	BoundGameState = nullptr;
	Super::NativeDestruct();
}

void ULB_MainMenuWaitingWidget::BindGameState()
{
	ALB_MainMenuGameState* NewGameState = GetWorld() ? GetWorld()->GetGameState<ALB_MainMenuGameState>() : nullptr;
	if (!IsValid(NewGameState))
	{
		return;
	}

	if (IsValid(BoundGameState))
	{
		BoundGameState->OnMainMenuSnapshotChanged.RemoveDynamic(this, &ThisClass::HandleSnapshotChanged);
	}
	BoundGameState = NewGameState;
	BoundGameState->OnMainMenuSnapshotChanged.AddUniqueDynamic(this, &ThisClass::HandleSnapshotChanged);
	Refresh(BoundGameState->GetMainMenuSnapshot());
}

void ULB_MainMenuWaitingWidget::HandleSnapshotChanged(const FLBMainMenuSnapshot& Snapshot)
{
	Refresh(Snapshot);
}

void ULB_MainMenuWaitingWidget::Refresh(const FLBMainMenuSnapshot& Snapshot)
{
	if (PlayerCountText.IsValid())
	{
		PlayerCountText->SetText(FText::Format(
			LOCTEXT("PlayersFormat", "Players: {0} / minimum {1}"),
			FText::AsNumber(Snapshot.ConnectedPlayers),
			FText::AsNumber(Snapshot.MinPlayersToStart)));
	}
	if (ConfirmedCountText.IsValid())
	{
		ConfirmedCountText->SetText(FText::Format(
			LOCTEXT("ConfirmedFormat", "Codenames confirmed: {0} / {1}"),
			FText::AsNumber(Snapshot.ConfirmedPlayers),
			FText::AsNumber(Snapshot.ConnectedPlayers)));
	}
	if (TargetMapText.IsValid())
	{
		TargetMapText->SetText(FText::Format(
			LOCTEXT("TargetFormat", "Target map: {0}"),
			FText::FromName(Snapshot.TargetMapName)));
	}
	if (PhaseText.IsValid())
	{
		FText PhaseLabel;
		switch (Snapshot.Phase)
		{
		case ELBMainMenuPhase::Ready:
			PhaseLabel = LOCTEXT("Ready", "Ready - the host can start the raid.");
			break;
		case ELBMainMenuPhase::Traveling:
			PhaseLabel = LOCTEXT("Traveling", "Traveling to the raid...");
			break;
		default:
			PhaseLabel = LOCTEXT("Collecting", "Waiting for every player to confirm a codename.");
			break;
		}
		PhaseText->SetText(PhaseLabel);
	}

	if (PlayerListBox.IsValid())
	{
		PlayerListBox->ClearChildren();
		if (IsValid(BoundGameState))
		{
			for (const TObjectPtr<APlayerState>& PlayerState : BoundGameState->PlayerArray)
			{
				if (!IsValid(PlayerState))
				{
					continue;
				}
				PlayerListBox->AddSlot().AutoHeight().Padding(0.f, 4.f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(PlayerState->GetPlayerName()))
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 17))
				];
			}
		}
	}

	if (StartButton.IsValid())
	{
		const ALB_MainMenuPlayerController* Controller = Cast<ALB_MainMenuPlayerController>(GetOwningPlayer());
		const bool bIsHost = IsValid(Controller) && Controller->IsLocalListenHost();
		StartButton->SetVisibility(bIsHost ? EVisibility::Visible : EVisibility::Collapsed);
		StartButton->SetEnabled(bIsHost && Snapshot.Phase == ELBMainMenuPhase::Ready && Controller->CanRequestStartHunt());
	}
}

FReply ULB_MainMenuWaitingWidget::HandleStartClicked()
{
	if (ALB_MainMenuPlayerController* Controller = Cast<ALB_MainMenuPlayerController>(GetOwningPlayer()))
	{
		Controller->RequestStartHunt();
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
