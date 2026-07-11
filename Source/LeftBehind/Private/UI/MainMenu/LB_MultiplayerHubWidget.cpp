#include "UI/MainMenu/LB_MultiplayerHubWidget.h"

#include "Player/LB_MainMenuPlayerController.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "LBMultiplayerHubWidget"

namespace
{
	FText GetDefaultOnlineStatus(const ELBOnlineState State, const bool bUsingEditorLanFallback)
	{
		if (bUsingEditorLanFallback && State == ELBOnlineState::Ready)
		{
			return LOCTEXT(
				"EditorLanReady",
				"Local LAN test mode. Configure EOS artifacts to browse internet rooms and invite friends.");
		}

		switch (State)
		{
		case ELBOnlineState::SignedOut:
			return LOCTEXT("SignedOut", "Sign in to Epic Online Services to browse rooms.");
		case ELBOnlineState::SigningIn:
			return LOCTEXT("SigningIn", "Signing in...");
		case ELBOnlineState::Ready:
			return LOCTEXT("Ready", "Online. Create a room or select one from the list.");
		case ELBOnlineState::Searching:
			return LOCTEXT("Searching", "Searching for available rooms...");
		case ELBOnlineState::Creating:
			return LOCTEXT("Creating", "Creating room...");
		case ELBOnlineState::Joining:
			return LOCTEXT("Joining", "Joining room...");
		case ELBOnlineState::InRoom:
			return LOCTEXT("InRoom", "Entering the room...");
		case ELBOnlineState::Leaving:
			return LOCTEXT("Leaving", "Leaving room...");
		case ELBOnlineState::Traveling:
			return LOCTEXT("Traveling", "Traveling with the party...");
		default:
			return LOCTEXT("Error", "The online request failed. You can retry sign-in.");
		}
	}
}

TSharedRef<SWidget> ULB_MultiplayerHubWidget::RebuildWidget()
{
	const FSlateFontInfo HeadingFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 30);
	const FSlateFontInfo BodyFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 17);

	return SNew(SBorder)
		.Padding(FMargin(36.f))
		.BorderBackgroundColor(FLinearColor(0.015f, 0.02f, 0.025f, 0.96f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Title", "MULTIPLAYER"))
				.Font(HeadingFont)
				.ColorAndOpacity(FLinearColor(0.86f, 0.91f, 0.95f))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 16.f)
			[
				SAssignNew(StatusText, STextBlock)
				.Font(BodyFont)
				.AutoWrapText(true)
			]
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				SNew(SBorder)
				.Padding(FMargin(12.f))
				.BorderBackgroundColor(FLinearColor(0.04f, 0.05f, 0.06f, 0.95f))
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SAssignNew(RoomListBox, SVerticalBox)
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 8.f)
			[
				SAssignNew(SelectionText, STextBlock)
				.Font(BodyFont)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
				[
					SAssignNew(SignInButton, SButton)
					.Text(LOCTEXT("SignIn", "RETRY SIGN IN"))
					.OnClicked_UObject(this, &ThisClass::HandleSignInClicked)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
				[
					SAssignNew(CreateButton, SButton)
					.Text(LOCTEXT("Create", "CREATE ROOM"))
					.OnClicked_UObject(this, &ThisClass::HandleCreateClicked)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
				[
					SAssignNew(RefreshButton, SButton)
					.Text(LOCTEXT("Refresh", "REFRESH"))
					.OnClicked_UObject(this, &ThisClass::HandleRefreshClicked)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
				[
					SAssignNew(JoinButton, SButton)
					.Text(LOCTEXT("Join", "JOIN SELECTED"))
					.OnClicked_UObject(this, &ThisClass::HandleJoinClicked)
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNullWidget::NullWidget
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SAssignNew(BackButton, SButton)
					.Text(LOCTEXT("Back", "BACK"))
					.OnClicked_UObject(this, &ThisClass::HandleBackClicked)
				]
			]
		];
}

void ULB_MultiplayerHubWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindOnlineSubsystem();
	RefreshFromSubsystem(true);
}

void ULB_MultiplayerHubWidget::NativeDestruct()
{
	if (IsValid(BoundOnlineSubsystem))
	{
		BoundOnlineSubsystem->OnStateChanged.RemoveDynamic(this, &ThisClass::HandleOnlineStateChanged);
		BoundOnlineSubsystem->OnRoomsChanged.RemoveDynamic(this, &ThisClass::HandleRoomsChanged);
	}
	BoundOnlineSubsystem = nullptr;
	Super::NativeDestruct();
}

void ULB_MultiplayerHubWidget::BindOnlineSubsystem()
{
	ULB_OnlineSessionSubsystem* OnlineSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<ULB_OnlineSessionSubsystem>()
		: nullptr;
	if (BoundOnlineSubsystem == OnlineSubsystem)
	{
		return;
	}

	if (IsValid(BoundOnlineSubsystem))
	{
		BoundOnlineSubsystem->OnStateChanged.RemoveDynamic(this, &ThisClass::HandleOnlineStateChanged);
		BoundOnlineSubsystem->OnRoomsChanged.RemoveDynamic(this, &ThisClass::HandleRoomsChanged);
	}

	BoundOnlineSubsystem = OnlineSubsystem;
	if (IsValid(BoundOnlineSubsystem))
	{
		BoundOnlineSubsystem->OnStateChanged.AddUniqueDynamic(this, &ThisClass::HandleOnlineStateChanged);
		BoundOnlineSubsystem->OnRoomsChanged.AddUniqueDynamic(this, &ThisClass::HandleRoomsChanged);
	}
}

void ULB_MultiplayerHubWidget::RefreshFromSubsystem(bool bRequestInitialSearch)
{
	if (!IsValid(BoundOnlineSubsystem))
	{
		if (StatusText.IsValid())
		{
			StatusText->SetText(LOCTEXT("Unavailable", "Epic Online Services is unavailable."));
			StatusText->SetColorAndOpacity(FLinearColor(1.f, 0.35f, 0.3f));
		}
		UpdateControls(ELBOnlineState::Error);
		return;
	}

	HandleRoomsChanged(BoundOnlineSubsystem->GetRooms());
	HandleOnlineStateChanged(BoundOnlineSubsystem->GetState(), FText::GetEmpty());
	if (bRequestInitialSearch && BoundOnlineSubsystem->GetState() == ELBOnlineState::Ready)
	{
		BoundOnlineSubsystem->RefreshRooms();
	}
}

void ULB_MultiplayerHubWidget::RefreshRoomRows()
{
	if (!RoomListBox.IsValid())
	{
		return;
	}

	RoomListBox->ClearChildren();
	if (Rooms.IsEmpty())
	{
		RoomListBox->AddSlot().AutoHeight().Padding(8.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoRooms", "No open rooms found. Refresh or create a new room."))
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 17))
		];
	}
	else
	{
		const bool bCanSelect = IsValid(BoundOnlineSubsystem)
			&& BoundOnlineSubsystem->GetState() == ELBOnlineState::Ready;
		for (const FLBRoomSummary& Room : Rooms)
		{
			const bool bSelected = Room.RoomId == SelectedRoomId;
			const FText RoomLabel = FText::Format(
				LOCTEXT("RoomFormat", "{0}    {1}/{2}{3}"),
				FText::FromString(Room.HostDisplayName.IsEmpty() ? TEXT("Unknown host") : Room.HostDisplayName),
				FText::AsNumber(Room.CurrentPlayers),
				FText::AsNumber(Room.MaxPlayers),
				Room.bCanJoin ? FText::GetEmpty() : LOCTEXT("RoomFull", "    FULL"));

			RoomListBox->AddSlot().AutoHeight().Padding(0.f, 3.f)
			[
				SNew(SButton)
				.IsEnabled(bCanSelect && Room.bCanJoin)
				.ButtonColorAndOpacity(bSelected
					? FLinearColor(0.13f, 0.34f, 0.48f, 1.f)
					: FLinearColor(0.09f, 0.1f, 0.12f, 1.f))
				.OnClicked_UObject(this, &ThisClass::HandleRoomSelected, Room.RoomId)
				[
					SNew(STextBlock)
					.Text(RoomLabel)
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 17))
				]
			];
		}
	}

	if (SelectionText.IsValid())
	{
		SelectionText->SetText(SelectedRoomId.IsEmpty()
			? LOCTEXT("NothingSelected", "Select an available room to join.")
			: LOCTEXT("RoomSelected", "Room selected."));
	}
}

void ULB_MultiplayerHubWidget::UpdateControls(ELBOnlineState State)
{
	const bool bReady = State == ELBOnlineState::Ready;
	const bool bCanRetry = State == ELBOnlineState::SignedOut || State == ELBOnlineState::Error;
	if (SignInButton.IsValid())
	{
		SignInButton->SetVisibility(bCanRetry ? EVisibility::Visible : EVisibility::Collapsed);
		SignInButton->SetEnabled(bCanRetry);
	}
	if (CreateButton.IsValid())
	{
		CreateButton->SetEnabled(bReady);
	}
	if (RefreshButton.IsValid())
	{
		RefreshButton->SetEnabled(bReady);
	}
	if (JoinButton.IsValid())
	{
		JoinButton->SetEnabled(bReady && !SelectedRoomId.IsEmpty());
	}
	if (BackButton.IsValid())
	{
		BackButton->SetEnabled(State != ELBOnlineState::Creating
			&& State != ELBOnlineState::Joining
			&& State != ELBOnlineState::Leaving
			&& State != ELBOnlineState::Traveling);
	}
	RefreshRoomRows();
}

void ULB_MultiplayerHubWidget::ShowRequestFailure(const FText& FallbackMessage)
{
	if (!StatusText.IsValid())
	{
		return;
	}

	const FText OnlineError = IsValid(BoundOnlineSubsystem)
		? BoundOnlineSubsystem->GetLastError()
		: FText::GetEmpty();
	StatusText->SetText(OnlineError.IsEmpty() ? FallbackMessage : OnlineError);
	StatusText->SetColorAndOpacity(FLinearColor(1.f, 0.35f, 0.3f));
}

void ULB_MultiplayerHubWidget::HandleOnlineStateChanged(
	ELBOnlineState NewState,
	const FText& StatusMessage)
{
	if (StatusText.IsValid())
	{
		FText EffectiveMessage = StatusMessage;
		if (EffectiveMessage.IsEmpty() && NewState == ELBOnlineState::Error && IsValid(BoundOnlineSubsystem))
		{
			EffectiveMessage = BoundOnlineSubsystem->GetLastError();
		}
		const bool bUsingEditorLanFallback = IsValid(BoundOnlineSubsystem)
			&& BoundOnlineSubsystem->IsUsingEditorLanFallback();
		StatusText->SetText(EffectiveMessage.IsEmpty()
			? GetDefaultOnlineStatus(NewState, bUsingEditorLanFallback)
			: EffectiveMessage);
		const bool bHasError = NewState == ELBOnlineState::Error
			|| (IsValid(BoundOnlineSubsystem) && !BoundOnlineSubsystem->GetLastError().IsEmpty());
		StatusText->SetColorAndOpacity(bHasError
			? FLinearColor(1.f, 0.35f, 0.3f)
			: FLinearColor(0.78f, 0.83f, 0.87f));
	}
	UpdateControls(NewState);
}

void ULB_MultiplayerHubWidget::HandleRoomsChanged(const TArray<FLBRoomSummary>& NewRooms)
{
	Rooms = NewRooms;
	const bool bSelectionStillValid = Rooms.ContainsByPredicate([this](const FLBRoomSummary& Room)
	{
		return Room.RoomId == SelectedRoomId && Room.bCanJoin;
	});
	if (!bSelectionStillValid)
	{
		SelectedRoomId.Reset();
	}
	RefreshRoomRows();
	if (JoinButton.IsValid())
	{
		JoinButton->SetEnabled(IsValid(BoundOnlineSubsystem)
			&& BoundOnlineSubsystem->GetState() == ELBOnlineState::Ready
			&& !SelectedRoomId.IsEmpty());
	}
}

FReply ULB_MultiplayerHubWidget::HandleRoomSelected(FString RoomId)
{
	SelectedRoomId = MoveTemp(RoomId);
	RefreshRoomRows();
	UpdateControls(IsValid(BoundOnlineSubsystem) ? BoundOnlineSubsystem->GetState() : ELBOnlineState::Error);
	return FReply::Handled();
}

FReply ULB_MultiplayerHubWidget::HandleSignInClicked()
{
	if (!IsValid(BoundOnlineSubsystem) || !BoundOnlineSubsystem->SignIn())
	{
		ShowRequestFailure(LOCTEXT("SignInRejected", "Sign-in could not be started."));
	}
	return FReply::Handled();
}

FReply ULB_MultiplayerHubWidget::HandleCreateClicked()
{
	if (!IsValid(BoundOnlineSubsystem) || !BoundOnlineSubsystem->CreateRoom())
	{
		ShowRequestFailure(LOCTEXT("CreateRejected", "Room creation could not be started."));
	}
	return FReply::Handled();
}

FReply ULB_MultiplayerHubWidget::HandleRefreshClicked()
{
	SelectedRoomId.Reset();
	if (!IsValid(BoundOnlineSubsystem) || !BoundOnlineSubsystem->RefreshRooms())
	{
		ShowRequestFailure(LOCTEXT("RefreshRejected", "Room search could not be started."));
	}
	return FReply::Handled();
}

FReply ULB_MultiplayerHubWidget::HandleJoinClicked()
{
	if (SelectedRoomId.IsEmpty()
		|| !IsValid(BoundOnlineSubsystem)
		|| !BoundOnlineSubsystem->JoinRoom(SelectedRoomId))
	{
		ShowRequestFailure(LOCTEXT("JoinRejected", "The selected room is no longer available. Refresh the list."));
	}
	return FReply::Handled();
}

FReply ULB_MultiplayerHubWidget::HandleBackClicked()
{
	if (ALB_MainMenuPlayerController* Controller = Cast<ALB_MainMenuPlayerController>(GetOwningPlayer()))
	{
		Controller->SetMenuScreen(ELBMainMenuScreen::Main);
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
