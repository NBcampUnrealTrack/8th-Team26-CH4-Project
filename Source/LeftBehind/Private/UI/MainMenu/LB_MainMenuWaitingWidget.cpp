#include "UI/MainMenu/LB_MainMenuWaitingWidget.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerState.h"
#include "GameState/LB_MainMenuGameState.h"
#include "Player/LB_PlayerState.h"
#include "Player/LB_MainMenuPlayerController.h"
#include "Styling/CoreStyle.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "LBMainMenuWaitingWidget"

namespace
{
	constexpr int32 LBSquadCapacity = 4;

	const FLinearColor LBAccentColor(0.95f, 0.43f, 0.12f, 1.f);
	const FLinearColor LBAccentHoverColor(1.f, 0.56f, 0.22f, 1.f);
	const FLinearColor LBTextColor(0.94f, 0.92f, 0.89f, 1.f);
	const FLinearColor LBMutedTextColor(0.58f, 0.57f, 0.55f, 1.f);
	const FLinearColor LBPanelColor(0.035f, 0.029f, 0.026f, 0.88f);
	const FLinearColor LBCardColor(0.075f, 0.062f, 0.054f, 0.92f);

	FSlateFontInfo MakeProjectFont(
		const UObject* FontAsset,
		const float Size,
		const FName Typeface,
		const bool bBoldFallback = false)
	{
		return FontAsset
			? FSlateFontInfo(FontAsset, Size, Typeface)
			: FCoreStyle::GetDefaultFontStyle(bBoldFallback ? TEXT("Bold") : TEXT("Regular"), Size);
	}
}

TSharedRef<SWidget> ULB_MainMenuWaitingWidget::RebuildWidget()
{
	TitleFontAsset = LoadObject<UObject>(
		nullptr, TEXT("/Game/LeftBehind/UI/Assets/Fonts/F_Title.F_Title"));
	BodyFontAsset = LoadObject<UObject>(
		nullptr, TEXT("/Game/LeftBehind/UI/Assets/Fonts/F_Body.F_Body"));
	HudNumberFontAsset = LoadObject<UObject>(
		nullptr, TEXT("/Game/LeftBehind/UI/Assets/Fonts/F_HUDNumber.F_HUDNumber"));

	const FSlateFontInfo EyebrowFont = MakeProjectFont(BodyFontAsset, 12.f, TEXT("Bold"), true);
	const FSlateFontInfo RoomNameFont = MakeProjectFont(TitleFontAsset, 40.f, TEXT("Default"), true);
	const FSlateFontInfo BodyFont = MakeProjectFont(BodyFontAsset, 15.f, TEXT("Regular"));
	const FSlateFontInfo BodyBoldFont = MakeProjectFont(BodyFontAsset, 15.f, TEXT("Bold"), true);
	const FSlateFontInfo SmallFont = MakeProjectFont(BodyFontAsset, 11.f, TEXT("Medium"));
	const FSlateFontInfo NumberFont = MakeProjectFont(HudNumberFontAsset, 27.f, TEXT("SemiBold"), true);
	const FSlateFontInfo RosterNumberFont = MakeProjectFont(HudNumberFontAsset, 15.f, TEXT("SemiBold"), true);

	MainPanelBrush = MakeShared<FSlateRoundedBoxBrush>(
		LBPanelColor, 8.f, FLinearColor(0.28f, 0.18f, 0.12f, 0.8f), 1.f);
	CardBrush = MakeShared<FSlateRoundedBoxBrush>(
		LBCardColor, 5.f, FLinearColor(0.25f, 0.17f, 0.12f, 0.75f), 1.f);
	RosterRowBrush = MakeShared<FSlateRoundedBoxBrush>(
		FLinearColor(0.055f, 0.047f, 0.043f, 0.96f), 3.f,
		FLinearColor(0.18f, 0.14f, 0.11f, 0.8f), 1.f);
	WarningBrush = MakeShared<FSlateRoundedBoxBrush>(FLinearColor::White, 4.f);

	PrimaryButtonStyle = MakeShared<FButtonStyle>();
	PrimaryButtonStyle
		->SetNormal(FSlateRoundedBoxBrush(LBAccentColor, 3.f))
		.SetHovered(FSlateRoundedBoxBrush(LBAccentHoverColor, 3.f))
		.SetPressed(FSlateRoundedBoxBrush(FLinearColor(0.78f, 0.29f, 0.06f, 1.f), 3.f))
		.SetDisabled(FSlateRoundedBoxBrush(FLinearColor(0.25f, 0.20f, 0.17f, 0.9f), 3.f))
		.SetNormalForeground(FLinearColor(0.055f, 0.035f, 0.02f, 1.f))
		.SetHoveredForeground(FLinearColor(0.035f, 0.02f, 0.01f, 1.f))
		.SetPressedForeground(FLinearColor(0.035f, 0.02f, 0.01f, 1.f))
		.SetDisabledForeground(FLinearColor(0.48f, 0.44f, 0.41f, 1.f))
		.SetNormalPadding(FMargin(25.f, 10.f))
		.SetPressedPadding(FMargin(25.f, 11.f, 25.f, 9.f));

	SecondaryButtonStyle = MakeShared<FButtonStyle>();
	SecondaryButtonStyle
		->SetNormal(FSlateRoundedBoxBrush(
			FLinearColor(0.08f, 0.065f, 0.055f, 0.96f), 3.f,
			FLinearColor(0.43f, 0.26f, 0.16f, 1.f), 1.f))
		.SetHovered(FSlateRoundedBoxBrush(
			FLinearColor(0.14f, 0.09f, 0.06f, 0.98f), 3.f,
			LBAccentColor, 1.f))
		.SetPressed(FSlateRoundedBoxBrush(
			FLinearColor(0.18f, 0.10f, 0.055f, 1.f), 3.f,
			LBAccentHoverColor, 1.f))
		.SetDisabled(FSlateRoundedBoxBrush(
			FLinearColor(0.06f, 0.055f, 0.052f, 0.75f), 3.f,
			FLinearColor(0.16f, 0.14f, 0.13f, 0.7f), 1.f))
		.SetNormalForeground(LBTextColor)
		.SetHoveredForeground(FLinearColor::White)
		.SetPressedForeground(FLinearColor::White)
		.SetDisabledForeground(FLinearColor(0.34f, 0.32f, 0.3f, 1.f))
		.SetNormalPadding(FMargin(18.f, 10.f))
		.SetPressedPadding(FMargin(18.f, 11.f, 18.f, 9.f));

	TextButtonStyle = MakeShared<FButtonStyle>();
	TextButtonStyle
		->SetNormal(FSlateRoundedBoxBrush(FLinearColor::Transparent, 3.f))
		.SetHovered(FSlateRoundedBoxBrush(FLinearColor(0.16f, 0.07f, 0.04f, 0.7f), 3.f))
		.SetPressed(FSlateRoundedBoxBrush(FLinearColor(0.22f, 0.08f, 0.035f, 0.8f), 3.f))
		.SetDisabled(FSlateRoundedBoxBrush(FLinearColor::Transparent, 3.f))
		.SetNormalForeground(LBMutedTextColor)
		.SetHoveredForeground(LBAccentHoverColor)
		.SetPressedForeground(LBAccentColor)
		.SetDisabledForeground(FLinearColor(0.28f, 0.27f, 0.26f, 1.f))
		.SetNormalPadding(FMargin(14.f, 10.f))
		.SetPressedPadding(FMargin(14.f, 11.f, 14.f, 9.f));

	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.52f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
				.BorderBackgroundColor(FLinearColor(0.012f, 0.009f, 0.008f, 0.72f))
			]
			+ SHorizontalBox::Slot().FillWidth(0.20f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
				.BorderBackgroundColor(FLinearColor::Transparent)
			]
			+ SHorizontalBox::Slot().FillWidth(0.28f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
				.BorderBackgroundColor(FLinearColor::Transparent)
			]
		]
		+ SOverlay::Slot().VAlign(VAlign_Top)
		[
			SNew(SBox)
			.HeightOverride(2.f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
				.BorderBackgroundColor(LBAccentColor)
			]
		]
		+ SOverlay::Slot()
		.Padding(FMargin(28.f, 20.f, 28.f, 22.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.50f)
			[
				SNew(SBorder)
				.BorderImage(MainPanelBrush.Get())
				.BorderBackgroundColor(FLinearColor::Transparent)
				.Padding(FMargin(30.f, 26.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SBox)
							.WidthOverride(4.f)
							.HeightOverride(13.f)
							[
								SNew(SBorder)
								.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
								.BorderBackgroundColor(LBAccentColor)
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(9.f, 0.f, 0.f, 0.f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("WaitingRoomEyebrow", "WAITING ROOM"))
							.Font(EyebrowFont)
							.ColorAndOpacity(LBAccentColor)
						]
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
							SNew(SBox)
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SAssignNew(RoomRoleText, STextBlock)
							.Font(SmallFont)
							.ColorAndOpacity(LBMutedTextColor)
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
					[
						SNew(SBox)
						.HeightOverride(48.f)
						[
							SNew(SScaleBox)
							.Stretch(EStretch::ScaleToFit)
							.StretchDirection(EStretchDirection::DownOnly)
							.HAlign(HAlign_Left)
							.VAlign(VAlign_Center)
							[
								SAssignNew(RoomNameText, STextBlock)
								.Text(LOCTEXT("RoomNameFallback", "HUNT LOBBY"))
								.Font(RoomNameFont)
								.ColorAndOpacity(LBTextColor)
								.ShadowOffset(FVector2D(0.f, 2.f))
								.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.7f))
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 1.f, 0.f, 10.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("WaitingRoomSubtitle", "대원들이 준비되면 방장이 사냥을 시작할 수 있습니다."))
						.Font(BodyFont)
						.ColorAndOpacity(LBMutedTextColor)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 6.f, 0.f)
						[
							SNew(SBorder)
							.BorderImage(CardBrush.Get())
							.Padding(FMargin(13.f, 8.f))
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(STextBlock)
									.Text(LOCTEXT("SquadLabel", "SQUAD"))
									.Font(SmallFont)
									.ColorAndOpacity(LBMutedTextColor)
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 1.f, 0.f, 0.f)
								[
									SAssignNew(PlayerCountText, STextBlock)
									.Font(NumberFont)
									.ColorAndOpacity(LBTextColor)
								]
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(6.f, 0.f)
						[
							SNew(SBorder)
							.BorderImage(CardBrush.Get())
							.Padding(FMargin(13.f, 8.f))
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(STextBlock)
									.Text(LOCTEXT("CodenameLabel", "CODENAMES"))
									.Font(SmallFont)
									.ColorAndOpacity(LBMutedTextColor)
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 1.f, 0.f, 0.f)
								[
									SAssignNew(ConfirmedCountText, STextBlock)
									.Font(NumberFont)
									.ColorAndOpacity(LBTextColor)
								]
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.35f).Padding(6.f, 0.f, 0.f, 0.f)
						[
							SNew(SBorder)
							.BorderImage(CardBrush.Get())
							.Padding(FMargin(13.f, 8.f))
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(STextBlock)
									.Text(LOCTEXT("TargetMapLabel", "TARGET MAP"))
									.Font(SmallFont)
									.ColorAndOpacity(LBMutedTextColor)
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 7.f, 0.f, 0.f)
								[
									SAssignNew(TargetMapText, STextBlock)
									.Font(BodyBoldFont)
									.ColorAndOpacity(LBTextColor)
								]
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth()
							[
								SAssignNew(PhaseText, STextBlock)
								.Font(BodyBoldFont)
								.ColorAndOpacity(LBAccentHoverColor)
							]
							+ SHorizontalBox::Slot().FillWidth(1.f)
							[
								SNew(SBox)
							]
							+ SHorizontalBox::Slot().AutoWidth()
							[
								SAssignNew(ReadyPercentText, STextBlock)
								.Font(RosterNumberFont)
								.ColorAndOpacity(LBMutedTextColor)
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 5.f, 0.f, 0.f)
						[
							SNew(SBox)
							.HeightOverride(4.f)
							[
								SAssignNew(ReadyProgressBar, SProgressBar)
								.Percent(0.f)
								.FillColorAndOpacity(LBAccentColor)
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("RosterLabel", "SQUAD ROSTER"))
							.Font(EyebrowFont)
							.ColorAndOpacity(LBTextColor)
						]
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
							SNew(SBox)
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("RosterCapacity", "4 SLOTS"))
							.Font(SmallFont)
							.ColorAndOpacity(LBMutedTextColor)
						]
					]
					+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 6.f, 0.f, 0.f)
					[
						SNew(SBorder)
						.BorderImage(CardBrush.Get())
						.Clipping(EWidgetClipping::ClipToBounds)
						.Padding(FMargin(7.f, 4.f))
						[
							SNew(SScrollBox)
							.Orientation(Orient_Vertical)
							.ScrollBarVisibility(EVisibility::Collapsed)
							+ SScrollBox::Slot()
							[
								SAssignNew(PlayerListBox, SVerticalBox)
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 6.f, 0.f)
						[
							SAssignNew(InviteButton, SButton)
							.ButtonStyle(SecondaryButtonStyle.Get())
							.ContentPadding(0.f)
							.OnClicked_UObject(this, &ThisClass::HandleInviteClicked)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("Invite", "INVITE FRIENDS"))
								.Font(EyebrowFont)
								.ColorAndOpacity(FSlateColor::UseForeground())
							]
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SAssignNew(LeaveButton, SButton)
							.ButtonStyle(TextButtonStyle.Get())
							.ContentPadding(0.f)
							.OnClicked_UObject(this, &ThisClass::HandleLeaveClicked)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("Leave", "LEAVE ROOM"))
								.Font(EyebrowFont)
								.ColorAndOpacity(FSlateColor::UseForeground())
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
							SNew(SBox)
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SAssignNew(StartButton, SButton)
							.ButtonStyle(PrimaryButtonStyle.Get())
							.ContentPadding(0.f)
							.OnClicked_UObject(this, &ThisClass::HandleStartClicked)
							[
								SAssignNew(StartButtonText, STextBlock)
								.Text(LOCTEXT("Start", "START RAID  >"))
								.Font(EyebrowFont)
								.ColorAndOpacity(FSlateColor::UseForeground())
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 0.f)
					[
						SAssignNew(OnlineStatusBorder, SBorder)
						.Visibility(EVisibility::Collapsed)
						.BorderImage(WarningBrush.Get())
						.BorderBackgroundColor(FLinearColor(0.36f, 0.17f, 0.055f, 0.82f))
						.Padding(FMargin(12.f, 8.f))
						[
							SAssignNew(OnlineStatusText, STextBlock)
							.Font(BodyFont)
							.ColorAndOpacity(FLinearColor(1.f, 0.73f, 0.48f, 1.f))
							.AutoWrapText(true)
						]
					]
				]
			]
			+ SHorizontalBox::Slot().FillWidth(0.50f)
			[
				SNew(SBox)
			]
		];
}

void ULB_MainMenuWaitingWidget::ReleaseSlateResources(const bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	RoomNameText.Reset();
	RoomRoleText.Reset();
	PlayerCountText.Reset();
	ConfirmedCountText.Reset();
	TargetMapText.Reset();
	PhaseText.Reset();
	ReadyPercentText.Reset();
	OnlineStatusText.Reset();
	OnlineStatusBorder.Reset();
	ReadyProgressBar.Reset();
	PlayerListBox.Reset();
	StartButton.Reset();
	StartButtonText.Reset();
	InviteButton.Reset();
	LeaveButton.Reset();
	PrimaryButtonStyle.Reset();
	SecondaryButtonStyle.Reset();
	TextButtonStyle.Reset();
	MainPanelBrush.Reset();
	CardBrush.Reset();
	RosterRowBrush.Reset();
	WarningBrush.Reset();
}

void ULB_MainMenuWaitingWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindGameState();
	BindOnlineSubsystem();
}

void ULB_MainMenuWaitingWidget::NativeDestruct()
{
	UnbindPlayerStateDelegates();
	if (IsValid(BoundGameState))
	{
		BoundGameState->OnMainMenuSnapshotChanged.RemoveDynamic(this, &ThisClass::HandleSnapshotChanged);
	}
	if (IsValid(BoundOnlineSubsystem))
	{
		BoundOnlineSubsystem->OnStateChanged.RemoveDynamic(this, &ThisClass::HandleOnlineStateChanged);
	}
	BoundGameState = nullptr;
	BoundOnlineSubsystem = nullptr;
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
		UnbindPlayerStateDelegates();
		BoundGameState->OnMainMenuSnapshotChanged.RemoveDynamic(this, &ThisClass::HandleSnapshotChanged);
	}
	BoundGameState = NewGameState;
	BoundGameState->OnMainMenuSnapshotChanged.AddUniqueDynamic(this, &ThisClass::HandleSnapshotChanged);
	Refresh(BoundGameState->GetMainMenuSnapshot());
}

void ULB_MainMenuWaitingWidget::BindOnlineSubsystem()
{
	ULB_OnlineSessionSubsystem* NewSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<ULB_OnlineSessionSubsystem>()
		: nullptr;
	if (BoundOnlineSubsystem == NewSubsystem)
	{
		return;
	}

	if (IsValid(BoundOnlineSubsystem))
	{
		BoundOnlineSubsystem->OnStateChanged.RemoveDynamic(this, &ThisClass::HandleOnlineStateChanged);
	}
	BoundOnlineSubsystem = NewSubsystem;
	if (IsValid(BoundOnlineSubsystem))
	{
		BoundOnlineSubsystem->OnStateChanged.AddUniqueDynamic(this, &ThisClass::HandleOnlineStateChanged);
		RefreshOnlineControls(BoundOnlineSubsystem->GetState(), FText::GetEmpty());
	}
	else
	{
		RefreshOnlineControls(ELBOnlineState::Error, LOCTEXT("OnlineUnavailable", "Online room controls are unavailable."));
	}

	RefreshRoomIdentity();
}

void ULB_MainMenuWaitingWidget::HandleSnapshotChanged(const FLBMainMenuSnapshot& Snapshot)
{
	Refresh(Snapshot);
}

void ULB_MainMenuWaitingWidget::HandleOnlineStateChanged(
	ELBOnlineState NewState,
	const FText& StatusMessage)
{
	RefreshRoomIdentity();
	RefreshOnlineControls(NewState, StatusMessage);
	if (IsValid(BoundGameState))
	{
		Refresh(BoundGameState->GetMainMenuSnapshot());
	}
}

void ULB_MainMenuWaitingWidget::HandlePlayerLobbyStateChanged(const bool bValue)
{
	(void)bValue;
	if (IsValid(BoundGameState))
	{
		Refresh(BoundGameState->GetMainMenuSnapshot());
	}
}

void ULB_MainMenuWaitingWidget::RefreshPlayerStateBindings()
{
	TArray<ALB_PlayerState*> CurrentPlayerStates;
	if (IsValid(BoundGameState))
	{
		for (const TObjectPtr<APlayerState>& PlayerState : BoundGameState->PlayerArray)
		{
			if (ALB_PlayerState* LBPlayerState = Cast<ALB_PlayerState>(PlayerState))
			{
				CurrentPlayerStates.Add(LBPlayerState);
			}
		}
	}

	for (ALB_PlayerState* BoundPlayerState : BoundPlayerStates)
	{
		if (IsValid(BoundPlayerState) && !CurrentPlayerStates.Contains(BoundPlayerState))
		{
			BoundPlayerState->OnCodenameConfirmedChanged.RemoveDynamic(
				this,
				&ThisClass::HandlePlayerLobbyStateChanged);
			BoundPlayerState->OnLobbyReadyChanged.RemoveDynamic(
				this,
				&ThisClass::HandlePlayerLobbyStateChanged);
		}
	}

	BoundPlayerStates.Reset(CurrentPlayerStates.Num());
	for (ALB_PlayerState* PlayerState : CurrentPlayerStates)
	{
		PlayerState->OnCodenameConfirmedChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandlePlayerLobbyStateChanged);
		PlayerState->OnLobbyReadyChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandlePlayerLobbyStateChanged);
		BoundPlayerStates.Add(PlayerState);
	}
}

void ULB_MainMenuWaitingWidget::UnbindPlayerStateDelegates()
{
	for (ALB_PlayerState* PlayerState : BoundPlayerStates)
	{
		if (!IsValid(PlayerState))
		{
			continue;
		}
		PlayerState->OnCodenameConfirmedChanged.RemoveDynamic(
			this,
			&ThisClass::HandlePlayerLobbyStateChanged);
		PlayerState->OnLobbyReadyChanged.RemoveDynamic(
			this,
			&ThisClass::HandlePlayerLobbyStateChanged);
	}
	BoundPlayerStates.Reset();
}

void ULB_MainMenuWaitingWidget::RefreshRoomIdentity()
{
	FString DisplayRoomName = IsValid(BoundOnlineSubsystem)
		? BoundOnlineSubsystem->GetCurrentRoomName()
		: FString();
	if (DisplayRoomName.IsEmpty())
	{
		DisplayRoomName = LOCTEXT("RoomNameFallback", "HUNT LOBBY").ToString();
	}

	if (RoomNameText.IsValid())
	{
		RoomNameText->SetText(FText::FromString(DisplayRoomName));
	}
	if (RoomRoleText.IsValid())
	{
		const bool bIsHost = IsValid(BoundOnlineSubsystem) && BoundOnlineSubsystem->IsRoomHost();
		RoomRoleText->SetText(bIsHost
			? LOCTEXT("HostRoomRole", "HOST  /  PUBLIC ROOM")
			: LOCTEXT("MemberRoomRole", "SQUAD MEMBER  /  PUBLIC ROOM"));
	}
}

void ULB_MainMenuWaitingWidget::RefreshOnlineControls(
	ELBOnlineState State,
	const FText& StatusMessage)
{
	const bool bInRoom = IsValid(BoundOnlineSubsystem) && BoundOnlineSubsystem->IsInRoom();
	const bool bRoomControlsEnabled = bInRoom && State == ELBOnlineState::InRoom;
	FText InviteUnavailableReason;
	const bool bCanInvite = bRoomControlsEnabled
		&& BoundOnlineSubsystem->CanOpenSocialOverlay(&InviteUnavailableReason);
	if (InviteButton.IsValid())
	{
		InviteButton->SetVisibility(EVisibility::Visible);
		InviteButton->SetEnabled(bCanInvite);
		InviteButton->SetToolTipText(bCanInvite ? FText::GetEmpty() : InviteUnavailableReason);
	}
	if (LeaveButton.IsValid())
	{
		LeaveButton->SetEnabled(bRoomControlsEnabled);
	}
	if (OnlineStatusText.IsValid())
	{
		FText EffectiveMessage = StatusMessage;
		if (EffectiveMessage.IsEmpty() && State == ELBOnlineState::Error && IsValid(BoundOnlineSubsystem))
		{
			EffectiveMessage = BoundOnlineSubsystem->GetLastError();
		}
		if (EffectiveMessage.IsEmpty() && bRoomControlsEnabled && !bCanInvite)
		{
			EffectiveMessage = InviteUnavailableReason;
		}
		const bool bHasError = State == ELBOnlineState::Error
			|| (IsValid(BoundOnlineSubsystem) && !BoundOnlineSubsystem->GetLastError().IsEmpty());
		OnlineStatusText->SetText(EffectiveMessage);
		OnlineStatusText->SetColorAndOpacity(bHasError
			? FLinearColor(1.f, 0.48f, 0.39f)
			: FLinearColor(1.f, 0.73f, 0.48f));
		if (OnlineStatusBorder.IsValid())
		{
			OnlineStatusBorder->SetVisibility(
				EffectiveMessage.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible);
			OnlineStatusBorder->SetBorderBackgroundColor(bHasError
				? FLinearColor(0.40f, 0.075f, 0.055f, 0.88f)
				: FLinearColor(0.36f, 0.17f, 0.055f, 0.82f));
		}
	}
}

void ULB_MainMenuWaitingWidget::Refresh(const FLBMainMenuSnapshot& Snapshot)
{
	RefreshRoomIdentity();
	RefreshPlayerStateBindings();

	if (PlayerCountText.IsValid())
	{
		PlayerCountText->SetText(FText::Format(
			LOCTEXT("PlayersFormat", "{0} / {1}"),
			FText::AsNumber(Snapshot.ConnectedPlayers),
			FText::AsNumber(LBSquadCapacity)));
	}
	if (ConfirmedCountText.IsValid())
	{
		ConfirmedCountText->SetText(FText::Format(
			LOCTEXT("ConfirmedFormat", "{0} / {1}"),
			FText::AsNumber(Snapshot.ConfirmedPlayers),
			FText::AsNumber(Snapshot.ConnectedPlayers)));
	}
	if (TargetMapText.IsValid())
	{
		FString FriendlyMapName = Snapshot.TargetMapName.ToString();
		FriendlyMapName.RemoveFromStart(TEXT("L_"));
		FriendlyMapName.ReplaceInline(TEXT("_"), TEXT(" "));
		FriendlyMapName.ToUpperInline();
		TargetMapText->SetText(FriendlyMapName.IsEmpty()
			? LOCTEXT("TargetPending", "PENDING")
			: FText::FromString(FriendlyMapName));
	}
	if (PhaseText.IsValid())
	{
		FText PhaseLabel;
		FLinearColor PhaseColor = LBMutedTextColor;
		switch (Snapshot.Phase)
		{
		case ELBMainMenuPhase::Ready:
			PhaseLabel = LOCTEXT("Ready", "READY TO DEPLOY");
			PhaseColor = LBAccentHoverColor;
			break;
		case ELBMainMenuPhase::Traveling:
			PhaseLabel = LOCTEXT("Traveling", "DEPLOYING...");
			PhaseColor = FLinearColor(0.56f, 0.76f, 0.91f, 1.f);
			break;
		default:
			PhaseLabel = Snapshot.ConfirmedPlayers < Snapshot.ConnectedPlayers
				? LOCTEXT("Collecting", "CONFIRMING CODENAMES")
				: LOCTEXT("WaitingForSquad", "WAITING FOR SQUAD READY");
			break;
		}
		PhaseText->SetText(PhaseLabel);
		PhaseText->SetColorAndOpacity(PhaseColor);
	}

	const float ReadyRatio = Snapshot.RequiredReadyPlayers > 0
		? FMath::Clamp(
			static_cast<float>(Snapshot.ReadyPlayers) / static_cast<float>(Snapshot.RequiredReadyPlayers),
			0.f,
			1.f)
		: (Snapshot.ConnectedPlayers > 0
			&& Snapshot.ConfirmedPlayers == Snapshot.ConnectedPlayers ? 1.f : 0.f);
	if (ReadyProgressBar.IsValid())
	{
		ReadyProgressBar->SetPercent(TOptional<float>(ReadyRatio));
	}
	if (ReadyPercentText.IsValid())
	{
		ReadyPercentText->SetText(FText::Format(
			LOCTEXT("ReadyPercentFormat", "{0}% READY"),
			FText::AsNumber(FMath::RoundToInt(ReadyRatio * 100.f))));
	}

	if (PlayerListBox.IsValid())
	{
		PlayerListBox->ClearChildren();
		int32 ValidPlayerCount = 0;
		const FSlateFontInfo RosterNameFont = MakeProjectFont(BodyFontAsset, 13.f, TEXT("Medium"));
		const FSlateFontInfo RosterStatusFont = MakeProjectFont(BodyFontAsset, 10.f, TEXT("Bold"), true);
		const FSlateFontInfo RosterNumberFont = MakeProjectFont(
			HudNumberFontAsset, 14.f, TEXT("SemiBold"), true);
		if (IsValid(BoundGameState))
		{
			for (const TObjectPtr<APlayerState>& PlayerState : BoundGameState->PlayerArray)
			{
				if (!IsValid(PlayerState))
				{
					continue;
				}

				const int32 SlotIndex = ValidPlayerCount++;
				const ALB_PlayerState* LBPlayerState = Cast<ALB_PlayerState>(PlayerState);
				const bool bConfirmed = IsValid(LBPlayerState) && LBPlayerState->IsCodenameConfirmed();
				const bool bHostPlayer = Snapshot.HostPlayerId != INDEX_NONE
					&& PlayerState->GetPlayerId() == Snapshot.HostPlayerId;
				const bool bLobbyReady = IsValid(LBPlayerState) && LBPlayerState->IsLobbyReady();
				FText PlayerStatusText;
				FLinearColor StatusColor(0.38f, 0.37f, 0.35f, 1.f);
				if (!bConfirmed)
				{
					PlayerStatusText = LOCTEXT("PlayerConfirming", "CONFIRMING");
				}
				else if (bHostPlayer)
				{
					PlayerStatusText = LOCTEXT("PlayerHost", "HOST");
					StatusColor = LBAccentHoverColor;
				}
				else if (bLobbyReady)
				{
					PlayerStatusText = LOCTEXT("PlayerReady", "READY");
					StatusColor = LBAccentHoverColor;
				}
				else
				{
					PlayerStatusText = LOCTEXT("PlayerNotReady", "NOT READY");
				}
				const FString PlayerDisplayName = PlayerState->GetPlayerName().IsEmpty()
					? LOCTEXT("UnknownAgent", "UNKNOWN AGENT").ToString()
					: PlayerState->GetPlayerName();

				PlayerListBox->AddSlot().AutoHeight().Padding(0.f, 2.f)
				[
					SNew(SBorder)
					.BorderImage(RosterRowBrush.Get())
					.Padding(FMargin(9.f, 5.f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(FString::Printf(TEXT("%02d"), SlotIndex + 1)))
							.Font(RosterNumberFont)
							.ColorAndOpacity(LBAccentColor)
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f)
						[
							SNew(SBox)
							.WidthOverride(5.f)
							.HeightOverride(5.f)
							[
								SNew(SBorder)
								.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
								.BorderBackgroundColor(StatusColor)
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(PlayerDisplayName))
							.Font(RosterNameFont)
							.ColorAndOpacity(LBTextColor)
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(PlayerStatusText)
							.Font(RosterStatusFont)
							.ColorAndOpacity(StatusColor)
						]
					]
				];
			}
		}

		for (int32 SlotIndex = ValidPlayerCount; SlotIndex < LBSquadCapacity; ++SlotIndex)
		{
			PlayerListBox->AddSlot().AutoHeight().Padding(0.f, 2.f)
			[
				SNew(SBorder)
				.BorderImage(RosterRowBrush.Get())
				.BorderBackgroundColor(FLinearColor(0.55f, 0.55f, 0.55f, 0.5f))
				.Padding(FMargin(9.f, 5.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("%02d"), SlotIndex + 1)))
						.Font(RosterNumberFont)
						.ColorAndOpacity(FLinearColor(0.32f, 0.29f, 0.27f, 1.f))
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(15.f, 0.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("OpenSlot", "OPEN SLOT"))
						.Font(RosterNameFont)
						.ColorAndOpacity(FLinearColor(0.34f, 0.32f, 0.30f, 1.f))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SlotWaiting", "WAITING"))
						.Font(RosterStatusFont)
						.ColorAndOpacity(FLinearColor(0.30f, 0.28f, 0.27f, 1.f))
					]
				]
			];
		}
	}

	if (StartButton.IsValid())
	{
		const ALB_MainMenuPlayerController* Controller = Cast<ALB_MainMenuPlayerController>(GetOwningPlayer());
		const bool bIsHost = IsValid(Controller) && Controller->IsLocalListenHost();
		const ALB_PlayerState* LocalPlayerState = IsValid(Controller)
			? Controller->GetPlayerState<ALB_PlayerState>()
			: nullptr;
		const bool bLocalReady = IsValid(LocalPlayerState) && LocalPlayerState->IsLobbyReady();
		StartButton->SetVisibility(IsValid(Controller) ? EVisibility::Visible : EVisibility::Collapsed);
		StartButton->SetEnabled(bIsHost
			? Snapshot.Phase == ELBMainMenuPhase::Ready && Controller->CanRequestStartCharacterSelect()
			: IsValid(Controller) && Controller->CanRequestLobbyReady());
		if (StartButtonText.IsValid())
		{
			StartButtonText->SetText(bIsHost
				? LOCTEXT("Start", "START RAID  >")
				: (bLocalReady
					? LOCTEXT("CancelReady", "CANCEL READY")
					: LOCTEXT("SetReady", "READY  >")));
		}
	}
}

FReply ULB_MainMenuWaitingWidget::HandleStartClicked()
{
	if (ALB_MainMenuPlayerController* Controller = Cast<ALB_MainMenuPlayerController>(GetOwningPlayer()))
	{
		if (Controller->IsLocalListenHost())
		{
			Controller->RequestStartCharacterSelect();
		}
		else if (const ALB_PlayerState* PlayerState = Controller->GetPlayerState<ALB_PlayerState>())
		{
			Controller->RequestSetLobbyReady(!PlayerState->IsLobbyReady());
		}
	}
	return FReply::Handled();
}

FReply ULB_MainMenuWaitingWidget::HandleInviteClicked()
{
	if (!IsValid(BoundOnlineSubsystem) || !BoundOnlineSubsystem->OpenSocialOverlay())
	{
		FText ErrorMessage = IsValid(BoundOnlineSubsystem)
			? BoundOnlineSubsystem->GetLastError()
			: FText::GetEmpty();
		if (ErrorMessage.IsEmpty() && IsValid(BoundOnlineSubsystem))
		{
			BoundOnlineSubsystem->CanOpenSocialOverlay(&ErrorMessage);
		}
		RefreshOnlineControls(
			IsValid(BoundOnlineSubsystem) ? BoundOnlineSubsystem->GetState() : ELBOnlineState::Error,
			ErrorMessage.IsEmpty()
				? LOCTEXT("OverlayFailed", "The Epic friends overlay could not be opened.")
				: ErrorMessage);
	}
	return FReply::Handled();
}

FReply ULB_MainMenuWaitingWidget::HandleLeaveClicked()
{
	if (!IsValid(BoundOnlineSubsystem) || !BoundOnlineSubsystem->LeaveRoom())
	{
		RefreshOnlineControls(
			IsValid(BoundOnlineSubsystem) ? BoundOnlineSubsystem->GetState() : ELBOnlineState::Error,
			LOCTEXT("LeaveFailed", "The room could not be left. Please try again."));
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
