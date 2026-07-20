#include "UI/MainMenu/LB_RoomCreationWidget.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Player/LB_MainMenuPlayerController.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "LBRoomCreationWidget"

namespace
{
	const FLinearColor LBRoomAccentColor(0.95f, 0.43f, 0.12f, 1.f);
	const FLinearColor LBRoomAccentHoverColor(1.f, 0.56f, 0.22f, 1.f);
	const FLinearColor LBRoomTextColor(0.94f, 0.92f, 0.89f, 1.f);
	const FLinearColor LBRoomMutedTextColor(0.58f, 0.57f, 0.55f, 1.f);
	const FLinearColor LBRoomSuccessColor(0.38f, 0.78f, 0.60f, 1.f);
	const FLinearColor LBRoomErrorColor(0.95f, 0.32f, 0.22f, 1.f);
	const FLinearColor LBRoomPanelColor(0.035f, 0.029f, 0.026f, 0.96f);

	FSlateFontInfo MakeRoomCreationFont(
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

TSharedRef<SWidget> ULB_RoomCreationWidget::RebuildWidget()
{
	TitleFontAsset = LoadObject<UObject>(
		nullptr, TEXT("/Game/LeftBehind/UI/Assets/Fonts/F_Body.F_Body"));
	BodyFontAsset = LoadObject<UObject>(
		nullptr, TEXT("/Game/LeftBehind/UI/Assets/Fonts/F_Body.F_Body"));

	const FSlateFontInfo EyebrowFont = MakeRoomCreationFont(BodyFontAsset, 12.f, TEXT("Bold"), true);
	const FSlateFontInfo TitleFont = MakeRoomCreationFont(TitleFontAsset, 44.f, TEXT("Bold"), true);
	const FSlateFontInfo SectionFont = MakeRoomCreationFont(BodyFontAsset, 13.f, TEXT("Bold"), true);
	const FSlateFontInfo BodyFont = MakeRoomCreationFont(BodyFontAsset, 11.f, TEXT("Regular"));
	const FSlateFontInfo BodyBoldFont = MakeRoomCreationFont(BodyFontAsset, 13.f, TEXT("Bold"), true);
	const FSlateFontInfo InputFont = MakeRoomCreationFont(BodyFontAsset, 12.f, TEXT("Medium"));
	const FSlateFontInfo SmallFont = MakeRoomCreationFont(BodyFontAsset, 11.f, TEXT("Medium"));
	const FSlateFontInfo PreviewFont = MakeRoomCreationFont(TitleFontAsset, 20.f, TEXT("Default"), true);

	PanelBrush = MakeShared<FSlateRoundedBoxBrush>(
		LBRoomPanelColor, 8.f, FLinearColor(0.28f, 0.18f, 0.12f, 0.86f), 1.f);
	InputPanelBrush = MakeShared<FSlateRoundedBoxBrush>(
		FLinearColor(0.055f, 0.046f, 0.041f, 1.f), 4.f,
		FLinearColor(0.34f, 0.23f, 0.17f, 1.f), 1.f);
	PreviewBrush = MakeShared<FSlateRoundedBoxBrush>(
		FLinearColor(0.07f, 0.056f, 0.048f, 0.96f), 5.f,
		FLinearColor(0.28f, 0.18f, 0.12f, 0.85f), 1.f);
	InfoBrush = MakeShared<FSlateRoundedBoxBrush>(
		FLinearColor(0.055f, 0.047f, 0.043f, 0.88f), 4.f,
		FLinearColor(0.18f, 0.14f, 0.11f, 0.8f), 1.f);
	StatusBrush = MakeShared<FSlateRoundedBoxBrush>(FLinearColor::White, 4.f);

	PrimaryButtonStyle = MakeShared<FButtonStyle>();
	PrimaryButtonStyle
		->SetNormal(FSlateRoundedBoxBrush(LBRoomAccentColor, 3.f))
		.SetHovered(FSlateRoundedBoxBrush(LBRoomAccentHoverColor, 3.f))
		.SetPressed(FSlateRoundedBoxBrush(FLinearColor(0.78f, 0.29f, 0.06f, 1.f), 3.f))
		.SetDisabled(FSlateRoundedBoxBrush(FLinearColor(0.25f, 0.20f, 0.17f, 0.94f), 3.f))
		.SetNormalForeground(FLinearColor(0.055f, 0.035f, 0.02f, 1.f))
		.SetHoveredForeground(FLinearColor(0.035f, 0.02f, 0.01f, 1.f))
		.SetPressedForeground(FLinearColor(0.035f, 0.02f, 0.01f, 1.f))
		.SetDisabledForeground(FLinearColor(0.48f, 0.44f, 0.41f, 1.f))
		.SetNormalPadding(FMargin(24.f, 12.f))
		.SetPressedPadding(FMargin(24.f, 13.f, 24.f, 11.f));

	SecondaryButtonStyle = MakeShared<FButtonStyle>();
	SecondaryButtonStyle
		->SetNormal(FSlateRoundedBoxBrush(
			FLinearColor(0.08f, 0.065f, 0.055f, 0.96f), 3.f,
			FLinearColor(0.43f, 0.26f, 0.16f, 1.f), 1.f))
		.SetHovered(FSlateRoundedBoxBrush(
			FLinearColor(0.14f, 0.09f, 0.06f, 0.98f), 3.f,
			LBRoomAccentColor, 1.f))
		.SetPressed(FSlateRoundedBoxBrush(
			FLinearColor(0.18f, 0.10f, 0.055f, 1.f), 3.f,
			LBRoomAccentHoverColor, 1.f))
		.SetDisabled(FSlateRoundedBoxBrush(
			FLinearColor(0.06f, 0.055f, 0.052f, 0.75f), 3.f,
			FLinearColor(0.16f, 0.14f, 0.13f, 0.7f), 1.f))
		.SetNormalForeground(LBRoomTextColor)
		.SetHoveredForeground(FLinearColor::White)
		.SetPressedForeground(FLinearColor::White)
		.SetDisabledForeground(FLinearColor(0.34f, 0.32f, 0.3f, 1.f))
		.SetNormalPadding(FMargin(18.f, 12.f))
		.SetPressedPadding(FMargin(18.f, 13.f, 18.f, 11.f));

	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
			.BorderBackgroundColor(FLinearColor(0.008f, 0.006f, 0.005f, 0.68f))
		]
		+ SOverlay::Slot().VAlign(VAlign_Top)
		[
			SNew(SBox)
			.HeightOverride(2.f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
				.BorderBackgroundColor(LBRoomAccentColor)
			]
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(FMargin(36.f, 28.f))
		[
			SNew(SScaleBox)
			.Stretch(EStretch::ScaleToFit)
			.StretchDirection(EStretchDirection::DownOnly)
			[
				SNew(SBox)
				.WidthOverride(980.f)
				.HeightOverride(630.f)
				[
					SNew(SBorder)
					.BorderImage(PanelBrush.Get())
					.Padding(FMargin(0.f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(0.36f)
						[
							SNew(SBorder)
							.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
							.BorderBackgroundColor(FLinearColor(0.065f, 0.041f, 0.029f, 0.96f))
							.Padding(FMargin(32.f, 34.f))
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
											.BorderBackgroundColor(LBRoomAccentColor)
										]
									]
									+ SHorizontalBox::Slot().AutoWidth().Padding(9.f, 0.f, 0.f, 0.f)
									[
										SNew(STextBlock)
										.Text(LOCTEXT("RoomSetupEyebrow", "ROOM SETUP / 01"))
										.Font(EyebrowFont)
										.ColorAndOpacity(LBRoomAccentColor)
									]
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 0.f)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("RoomSetupTitle", "작전실\n개설"))
									.Font(TitleFont)
									.ColorAndOpacity(LBRoomTextColor)
									.LineHeightPercentage(0.86f)
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 20.f, 0.f, 0.f)
								[
									SNew(STextBlock)
									.Text(LOCTEXT(
										"RoomSetupDescription",
										"대원들이 찾기 쉬운 방 이름을 정하세요.\n생성 후 바로 대기실로 이동합니다."))
									.Font(BodyFont)
									.ColorAndOpacity(LBRoomMutedTextColor)
									.AutoWrapText(true)
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 40.f, 0.f, 12.f)
								[
									SNew(STextBlock)
									.Text(LOCTEXT("FixedRules", "FIXED PARAMETERS"))
									.Font(EyebrowFont)
									.ColorAndOpacity(LBRoomTextColor)
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
								[
									SNew(SBorder)
									.BorderImage(InfoBrush.Get())
									.Padding(FMargin(13.f, 10.f))
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().AutoWidth()
										[
											SNew(STextBlock)
											.Text(LOCTEXT("VisibilityKey", "VISIBILITY"))
											.Font(SmallFont)
											.ColorAndOpacity(LBRoomMutedTextColor)
										]
										+ SHorizontalBox::Slot().FillWidth(1.f)
										[
											SNew(SBox)
										]
										+ SHorizontalBox::Slot().AutoWidth()
										[
											SNew(STextBlock)
											.Text(LOCTEXT("VisibilityValue", "PUBLIC"))
											.Font(SectionFont)
											.ColorAndOpacity(LBRoomAccentHoverColor)
										]
									]
								]
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(SBorder)
									.BorderImage(InfoBrush.Get())
									.Padding(FMargin(13.f, 10.f))
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().AutoWidth()
										[
											SNew(STextBlock)
											.Text(LOCTEXT("CapacityKey", "SQUAD SIZE"))
											.Font(SmallFont)
											.ColorAndOpacity(LBRoomMutedTextColor)
										]
										+ SHorizontalBox::Slot().FillWidth(1.f)
										[
											SNew(SBox)
										]
										+ SHorizontalBox::Slot().AutoWidth()
										[
											SNew(STextBlock)
											.Text(LOCTEXT("CapacityValue", "4 PLAYERS"))
											.Font(SectionFont)
											.ColorAndOpacity(LBRoomTextColor)
										]
									]
								]
								+ SVerticalBox::Slot().FillHeight(1.f)
								[
									SNew(SBox)
								]
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(STextBlock)
									.Text(LOCTEXT("SecureService", "EOS SECURE SESSION"))
									.Font(SmallFont)
									.ColorAndOpacity(FLinearColor(0.42f, 0.48f, 0.46f, 1.f))
								]
							]
						]
						+ SHorizontalBox::Slot().FillWidth(0.64f)
						[
							SNew(SBorder)
							.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
							.BorderBackgroundColor(FLinearColor(0.025f, 0.021f, 0.019f, 0.98f))
							.Padding(FMargin(38.f, 34.f))
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().AutoWidth()
									[
										SNew(STextBlock)
										.Text(LOCTEXT("IdentitySection", "ROOM IDENTITY"))
										.Font(EyebrowFont)
										.ColorAndOpacity(LBRoomTextColor)
									]
									+ SHorizontalBox::Slot().FillWidth(1.f)
									[
										SNew(SBox)
									]
									+ SHorizontalBox::Slot().AutoWidth()
									[
										SNew(SBox)
										.MaxDesiredWidth(260.f)
										[
											SAssignNew(StatusBanner, SBorder)
											.BorderImage(StatusBrush.Get())
											.BorderBackgroundColor(FLinearColor(0.13f, 0.24f, 0.19f, 0.95f))
											.Padding(FMargin(10.f, 5.f))
											[
												SNew(SHorizontalBox)
												+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
												[
													SAssignNew(CreatingIndicator, SThrobber)
													.NumPieces(3)
													.PieceImage(FCoreStyle::Get().GetBrush(TEXT("Throbber.CircleChunk")))
													.Visibility(EVisibility::Collapsed)
												]
												+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(5.f, 0.f, 0.f, 0.f)
												[
													SAssignNew(StatusText, STextBlock)
													.Text(LOCTEXT("OnlineReady", "ONLINE / READY"))
													.Font(SmallFont)
													.ColorAndOpacity(LBRoomSuccessColor)
													.AutoWrapText(true)
													.WrapTextAt(220.f)
												]
											]
										]
									]
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 34.f, 0.f, 0.f)
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().AutoWidth()
									[
										SNew(STextBlock)
										.Text(LOCTEXT("RoomNameLabel", "방 이름"))
										.Font(BodyBoldFont)
										.ColorAndOpacity(LBRoomTextColor)
									]
									+ SHorizontalBox::Slot().FillWidth(1.f)
									[
										SNew(SBox)
									]
									+ SHorizontalBox::Slot().AutoWidth()
									[
										SAssignNew(CharacterCountText, STextBlock)
										.Text(LOCTEXT("EmptyCharacterCount", "0 / 24"))
										.Font(SectionFont)
										.ColorAndOpacity(LBRoomMutedTextColor)
									]
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 9.f, 0.f, 0.f)
								[
									SNew(SBox)
									.HeightOverride(58.f)
									[
										SNew(SBorder)
										.BorderImage(InputPanelBrush.Get())
										.Padding(FMargin(14.f, 6.f))
										[
											SAssignNew(RoomNameInput, SEditableTextBox)
											.Text(FText::GetEmpty())
											.HintText(LOCTEXT("RoomNameHint", "예: 마지막 생존자들"))
											.Font(InputFont)
											.ForegroundColor(LBRoomTextColor)
											.BackgroundColor(FLinearColor::Transparent)
											.ClearKeyboardFocusOnCommit(false)
											.SelectAllTextWhenFocused(false)
											.OnTextChanged_UObject(this, &ThisClass::HandleRoomNameChanged)
											.OnTextCommitted_UObject(this, &ThisClass::HandleRoomNameCommitted)
										]
									]
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
								[
									SAssignNew(ValidationText, STextBlock)
									.Text(LOCTEXT("RoomNameRequired", "2~24자의 방 이름을 입력하세요."))
									.Font(SmallFont)
									.ColorAndOpacity(LBRoomMutedTextColor)
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 18.f, 0.f, 0.f)
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 7.f, 0.f)
									[
										SNew(SBorder)
										.BorderImage(InfoBrush.Get())
										.Padding(FMargin(9.f, 5.f))
										[
											SNew(STextBlock)
											.Text(LOCTEXT("LengthRule", "2–24 CHARACTERS"))
											.Font(SmallFont)
											.ColorAndOpacity(LBRoomMutedTextColor)
										]
									]
									+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 7.f, 0.f)
									[
										SNew(SBorder)
										.BorderImage(InfoBrush.Get())
										.Padding(FMargin(9.f, 5.f))
										[
											SNew(STextBlock)
											.Text(LOCTEXT("CharacterRule", "한글 · ENG · 0–9"))
											.Font(SmallFont)
											.ColorAndOpacity(LBRoomMutedTextColor)
										]
									]
									+ SHorizontalBox::Slot().AutoWidth()
									[
										SNew(SBorder)
										.BorderImage(InfoBrush.Get())
										.Padding(FMargin(9.f, 5.f))
										[
											SNew(STextBlock)
											.Text(LOCTEXT("WhitespaceRule", "공백 자동 제거"))
											.Font(SmallFont)
											.ColorAndOpacity(LBRoomMutedTextColor)
										]
									]
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 25.f, 0.f, 0.f)
								[
									SNew(SBorder)
									.BorderImage(PreviewBrush.Get())
									.Padding(FMargin(18.f, 14.f))
									[
										SNew(SVerticalBox)
										+ SVerticalBox::Slot().AutoHeight()
										[
											SNew(STextBlock)
											.Text(LOCTEXT("RoomPreviewLabel", "ROOM LIST PREVIEW"))
											.Font(SmallFont)
											.ColorAndOpacity(LBRoomMutedTextColor)
										]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 5.f, 0.f, 0.f)
										[
											SNew(SScaleBox)
											.Stretch(EStretch::ScaleToFit)
											.StretchDirection(EStretchDirection::DownOnly)
											.HAlign(HAlign_Left)
											[
												SAssignNew(PreviewNameText, STextBlock)
												.Text(LOCTEXT("RoomPreviewEmpty", "방 이름 미리보기"))
												.Font(PreviewFont)
												.ColorAndOpacity(LBRoomTextColor)
											]
										]
									]
								]
								+ SVerticalBox::Slot().FillHeight(1.f)
								[
									SNew(SBox)
								]
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().AutoWidth()
									[
										SAssignNew(BackButton, SButton)
										.ButtonStyle(SecondaryButtonStyle.Get())
										.OnClicked_UObject(this, &ThisClass::HandleBackClicked)
										[
											SNew(STextBlock)
											.Text(LOCTEXT("BackToRooms", "<  방 목록"))
											.Font(SectionFont)
										]
									]
									+ SHorizontalBox::Slot().FillWidth(1.f)
									[
										SNew(SBox)
									]
									+ SHorizontalBox::Slot().AutoWidth()
									[
										SAssignNew(CreateButton, SButton)
										.ButtonStyle(PrimaryButtonStyle.Get())
										.IsEnabled(false)
										.OnClicked_UObject(this, &ThisClass::HandleCreateClicked)
										[
											SNew(STextBlock)
											.Text(LOCTEXT("CreateRoom", "방 만들기  >"))
											.Font(BodyBoldFont)
										]
									]
								]
							]
						]
					]
				]
			]
		];
}

void ULB_RoomCreationWidget::ReleaseSlateResources(const bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	RoomNameInput.Reset();
	CharacterCountText.Reset();
	ValidationText.Reset();
	PreviewNameText.Reset();
	StatusText.Reset();
	StatusBanner.Reset();
	CreateButton.Reset();
	BackButton.Reset();
	CreatingIndicator.Reset();
	PrimaryButtonStyle.Reset();
	SecondaryButtonStyle.Reset();
	PanelBrush.Reset();
	InputPanelBrush.Reset();
	PreviewBrush.Reset();
	InfoBrush.Reset();
	StatusBrush.Reset();
}

void ULB_RoomCreationWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindOnlineSubsystem();
	if (RoomNameInput.IsValid())
	{
		RoomNameInput->SetText(FText::GetEmpty());
	}
	RefreshDraft(FText::GetEmpty());

	const ELBOnlineState State = IsValid(BoundOnlineSubsystem)
		? BoundOnlineSubsystem->GetState()
		: ELBOnlineState::Error;
	HandleOnlineStateChanged(State, FText::GetEmpty());

	if (RoomNameInput.IsValid() && State == ELBOnlineState::Ready && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetKeyboardFocus(RoomNameInput, EFocusCause::SetDirectly);
	}
}

void ULB_RoomCreationWidget::NativeDestruct()
{
	if (IsValid(BoundOnlineSubsystem))
	{
		BoundOnlineSubsystem->OnStateChanged.RemoveDynamic(this, &ThisClass::HandleOnlineStateChanged);
	}
	BoundOnlineSubsystem = nullptr;
	NormalizedDraft.Reset();
	bDraftValid = false;
	bCreating = false;
	Super::NativeDestruct();
}

FReply ULB_RoomCreationWidget::NativeOnPreviewKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (!bCreating)
		{
			ReturnToRoomBrowser();
		}
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply ULB_RoomCreationWidget::NativeOnKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape && !bCreating)
	{
		ReturnToRoomBrowser();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void ULB_RoomCreationWidget::BindOnlineSubsystem()
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
	}
	BoundOnlineSubsystem = OnlineSubsystem;
	if (IsValid(BoundOnlineSubsystem))
	{
		BoundOnlineSubsystem->OnStateChanged.AddUniqueDynamic(this, &ThisClass::HandleOnlineStateChanged);
	}
}

void ULB_RoomCreationWidget::RefreshDraft(const FText& Draft)
{
	const FString RawDraft = Draft.ToString();
	FString CompactDraft;
	CompactDraft.Reserve(RawDraft.Len());
	for (const TCHAR Character : RawDraft)
	{
		if (!FChar::IsWhitespace(Character))
		{
			CompactDraft.AppendChar(Character);
		}
	}

	FText ValidationError;
	bDraftValid = ULB_OnlineSessionSubsystem::ValidateRoomName(
		RawDraft, NormalizedDraft, ValidationError);

	if (CharacterCountText.IsValid())
	{
		CharacterCountText->SetText(FText::Format(
			LOCTEXT("RoomNameCharacterCount", "{0} / 24"),
			FText::AsNumber(CompactDraft.Len())));
		CharacterCountText->SetColorAndOpacity(
			CompactDraft.Len() > 24 ? LBRoomErrorColor : LBRoomMutedTextColor);
	}

	if (PreviewNameText.IsValid())
	{
		PreviewNameText->SetText(CompactDraft.IsEmpty()
			? LOCTEXT("RoomPreviewEmpty", "방 이름 미리보기")
			: FText::FromString(CompactDraft));
		PreviewNameText->SetColorAndOpacity(
			CompactDraft.IsEmpty() ? LBRoomMutedTextColor : LBRoomTextColor);
	}

	if (ValidationText.IsValid())
	{
		if (CompactDraft.IsEmpty())
		{
			ValidationText->SetText(LOCTEXT("RoomNameRequired", "2~24자의 방 이름을 입력하세요."));
			ValidationText->SetColorAndOpacity(LBRoomMutedTextColor);
		}
		else if (bDraftValid)
		{
			ValidationText->SetText(LOCTEXT("RoomNameAvailable", "사용할 수 있는 방 이름입니다."));
			ValidationText->SetColorAndOpacity(LBRoomSuccessColor);
		}
		else
		{
			ValidationText->SetText(ValidationError);
			ValidationText->SetColorAndOpacity(LBRoomErrorColor);
		}
	}

	RefreshControls(IsValid(BoundOnlineSubsystem)
		? BoundOnlineSubsystem->GetState()
		: ELBOnlineState::Error);
}

void ULB_RoomCreationWidget::RefreshControls(const ELBOnlineState State)
{
	bCreating = State == ELBOnlineState::Creating || State == ELBOnlineState::Traveling;
	if (RoomNameInput.IsValid())
	{
		RoomNameInput->SetIsReadOnly(bCreating);
	}
	if (CreateButton.IsValid())
	{
		CreateButton->SetEnabled(State == ELBOnlineState::Ready && bDraftValid);
	}
	if (BackButton.IsValid())
	{
		BackButton->SetEnabled(!bCreating);
	}
	if (CreatingIndicator.IsValid())
	{
		CreatingIndicator->SetVisibility(bCreating ? EVisibility::Visible : EVisibility::Collapsed);
	}
}

void ULB_RoomCreationWidget::ShowStatus(const FText& Message, const bool bIsError)
{
	if (StatusText.IsValid())
	{
		StatusText->SetText(Message);
		StatusText->SetColorAndOpacity(bIsError ? LBRoomErrorColor : LBRoomSuccessColor);
	}
	if (StatusBanner.IsValid())
	{
		StatusBanner->SetBorderBackgroundColor(bIsError
			? FLinearColor(0.29f, 0.075f, 0.045f, 0.96f)
			: FLinearColor(0.13f, 0.24f, 0.19f, 0.95f));
	}
}

void ULB_RoomCreationWidget::ReturnToRoomBrowser()
{
	if (ALB_MainMenuPlayerController* Controller =
		Cast<ALB_MainMenuPlayerController>(GetOwningPlayer()))
	{
		Controller->CancelRoomCreation();
	}
}

bool ULB_RoomCreationWidget::CanSubmitDraft() const
{
	return bDraftValid
		&& !bCreating
		&& IsValid(BoundOnlineSubsystem)
		&& BoundOnlineSubsystem->GetState() == ELBOnlineState::Ready;
}

void ULB_RoomCreationWidget::HandleRoomNameChanged(const FText& Text)
{
	RefreshDraft(Text);
	if (IsValid(BoundOnlineSubsystem)
		&& BoundOnlineSubsystem->GetState() == ELBOnlineState::Ready)
	{
		ShowStatus(LOCTEXT("OnlineReady", "ONLINE / READY"), false);
	}
}

void ULB_RoomCreationWidget::HandleRoomNameCommitted(
	const FText& Text,
	const ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter && CanSubmitDraft())
	{
		HandleCreateClicked();
	}
}

void ULB_RoomCreationWidget::HandleOnlineStateChanged(
	const ELBOnlineState NewState,
	const FText& StatusMessage)
{
	RefreshControls(NewState);

	if (NewState == ELBOnlineState::Creating)
	{
		ShowStatus(LOCTEXT("CreatingRoom", "CREATING ROOM..."), false);
		return;
	}
	if (NewState == ELBOnlineState::Traveling)
	{
		ShowStatus(LOCTEXT("EnteringRoom", "ENTERING WAITING ROOM..."), false);
		return;
	}
	if (!StatusMessage.IsEmpty())
	{
		ShowStatus(StatusMessage, true);
		return;
	}
	if (NewState == ELBOnlineState::Ready)
	{
		ShowStatus(LOCTEXT("OnlineReady", "ONLINE / READY"), false);
		return;
	}

	const FText LastError = IsValid(BoundOnlineSubsystem)
		? BoundOnlineSubsystem->GetLastError()
		: FText::GetEmpty();
	ShowStatus(
		LastError.IsEmpty()
			? LOCTEXT("OnlineUnavailable", "ONLINE SERVICE UNAVAILABLE")
			: LastError,
		true);
}

FReply ULB_RoomCreationWidget::HandleCreateClicked()
{
	if (!CanSubmitDraft())
	{
		return FReply::Handled();
	}

	ALB_MainMenuPlayerController* Controller =
		Cast<ALB_MainMenuPlayerController>(GetOwningPlayer());
	if (!IsValid(Controller) || !Controller->SubmitRoomName(FText::FromString(NormalizedDraft)))
	{
		const FText LastError = IsValid(BoundOnlineSubsystem)
			? BoundOnlineSubsystem->GetLastError()
			: FText::GetEmpty();
		ShowStatus(
			LastError.IsEmpty()
				? LOCTEXT("CreateRejected", "방 만들기를 시작할 수 없습니다.")
				: LastError,
			true);
		RefreshControls(IsValid(BoundOnlineSubsystem)
			? BoundOnlineSubsystem->GetState()
			: ELBOnlineState::Error);
	}
	return FReply::Handled();
}

FReply ULB_RoomCreationWidget::HandleBackClicked()
{
	if (!bCreating)
	{
		ReturnToRoomBrowser();
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
