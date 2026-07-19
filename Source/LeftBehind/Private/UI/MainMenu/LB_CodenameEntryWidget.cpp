#include "UI/MainMenu/LB_CodenameEntryWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableText.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/GameInstance.h"
#include "GameMode/LB_MainMenuGameMode.h"
#include "Player/LB_MainMenuPlayerController.h"
#include "System/MainMenu/LB_LocalPlayerProfileSubsystem.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "LBCodenameEntryWidget"

namespace
{
	FMulticastDelegateProperty* FindClickDelegate(UWidget* Widget)
	{
		if (!IsValid(Widget))
		{
			return nullptr;
		}
		if (FMulticastDelegateProperty* CustomDelegate = FindFProperty<FMulticastDelegateProperty>(Widget->GetClass(), TEXT("OnBTNClicked")))
		{
			return CustomDelegate;
		}
		return FindFProperty<FMulticastDelegateProperty>(Widget->GetClass(), TEXT("OnClicked"));
	}

	void BindClick(UWidget* Widget, UObject* Handler, FName FunctionName)
	{
		if (FMulticastDelegateProperty* Property = FindClickDelegate(Widget))
		{
			void* Value = Property->ContainerPtrToValuePtr<void>(Widget);
			Property->ClearDelegate(Widget, Value);
			FScriptDelegate Delegate;
			Delegate.BindUFunction(Handler, FunctionName);
			Property->AddDelegate(Delegate, Widget, Value);
		}
	}

	void UnbindClick(UWidget* Widget, UObject* Handler, FName FunctionName)
	{
		if (FMulticastDelegateProperty* Property = FindClickDelegate(Widget))
		{
			FScriptDelegate Delegate;
			Delegate.BindUFunction(Handler, FunctionName);
			Property->RemoveDelegate(Delegate, Widget, Property->ContainerPtrToValuePtr<void>(Widget));
		}
	}
}

void ULB_CodenameEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ConfirmButton = WidgetTree ? WidgetTree->FindWidget(TEXT("BTN_Confirm")) : nullptr;
	BackButton = WidgetTree ? WidgetTree->FindWidget(TEXT("BTN_CodeName_Back")) : nullptr;
	NameInput = WidgetTree ? Cast<UEditableText>(WidgetTree->FindWidget(TEXT("ETB_Name"))) : nullptr;
	CharacterCountText = WidgetTree
		? Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("TXT_CharCount")))
		: nullptr;
	ALB_MainMenuPlayerController* Controller = Cast<ALB_MainMenuPlayerController>(GetOwningPlayer());
	bRoomNameMode = IsValid(Controller) && Controller->IsRoomNameEntryActive();
	if (bRoomNameMode)
	{
		ConfigureRoomNamePresentation();
	}

	if (IsValid(ConfirmButton))
	{
		BindClick(ConfirmButton, this, GET_FUNCTION_NAME_CHECKED(ThisClass, HandleConfirmClicked));
	}
	if (IsValid(BackButton))
	{
		BindClick(BackButton, this, GET_FUNCTION_NAME_CHECKED(ThisClass, HandleBackClicked));
	}
	if (IsValid(NameInput))
	{
		NameInput->OnTextChanged.Clear();
		NameInput->OnTextCommitted.Clear();
		NameInput->OnTextChanged.AddUniqueDynamic(this, &ThisClass::HandleTextChanged);
		NameInput->OnTextCommitted.AddUniqueDynamic(this, &ThisClass::HandleTextCommitted);
		if (bRoomNameMode)
		{
			NameInput->SetText(FText::GetEmpty());
		}
		else if (const UGameInstance* GameInstance = GetGameInstance())
		{
			if (const ULB_LocalPlayerProfileSubsystem* Profile =
				GameInstance->GetSubsystem<ULB_LocalPlayerProfileSubsystem>();
				IsValid(Profile) && Profile->HasCodename())
			{
				NameInput->SetText(FText::FromString(Profile->GetCodename()));
			}
		}
		HandleTextChanged(NameInput->GetText());
		NameInput->SetKeyboardFocus();
	}

	if (IsValid(Controller))
	{
		Controller->OnCodenameSubmissionResult.AddUniqueDynamic(this, &ThisClass::HandleSubmissionResult);
	}
	if (bRoomNameMode)
	{
		if (ULB_OnlineSessionSubsystem* OnlineSubsystem = GetGameInstance()
			? GetGameInstance()->GetSubsystem<ULB_OnlineSessionSubsystem>()
			: nullptr)
		{
			OnlineSubsystem->OnStateChanged.AddUniqueDynamic(
				this, &ThisClass::HandleOnlineStateChanged);
		}
	}
	
	BP_OnWidgetShown();
}

void ULB_CodenameEntryWidget::NativeDestruct()
{
	if (IsValid(ConfirmButton))
	{
		UnbindClick(ConfirmButton, this, GET_FUNCTION_NAME_CHECKED(ThisClass, HandleConfirmClicked));
	}
	if (IsValid(BackButton))
	{
		UnbindClick(BackButton, this, GET_FUNCTION_NAME_CHECKED(ThisClass, HandleBackClicked));
	}
	if (IsValid(NameInput))
	{
		NameInput->OnTextChanged.RemoveDynamic(this, &ThisClass::HandleTextChanged);
		NameInput->OnTextCommitted.RemoveDynamic(this, &ThisClass::HandleTextCommitted);
	}
	if (ALB_MainMenuPlayerController* Controller = Cast<ALB_MainMenuPlayerController>(GetOwningPlayer()))
	{
		Controller->OnCodenameSubmissionResult.RemoveDynamic(this, &ThisClass::HandleSubmissionResult);
	}
	if (ULB_OnlineSessionSubsystem* OnlineSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<ULB_OnlineSessionSubsystem>()
		: nullptr)
	{
		OnlineSubsystem->OnStateChanged.RemoveDynamic(
			this, &ThisClass::HandleOnlineStateChanged);
	}

	ConfirmButton = nullptr;
	BackButton = nullptr;
	NameInput = nullptr;
	ErrorText = nullptr;
	CharacterCountText = nullptr;
	bRoomNameMode = false;
	Super::NativeDestruct();
}

void ULB_CodenameEntryWidget::HandleConfirmClicked()
{
	BP_OnConfirmClicked();
	//SubmitCurrentText();
}

void ULB_CodenameEntryWidget::HandleBackClicked()
{
	if (ALB_MainMenuPlayerController* Controller = Cast<ALB_MainMenuPlayerController>(GetOwningPlayer()))
	{
		if (Controller->CancelCodenameEntry())
		{
			return;
		}

		FInputModeGameAndUI InputMode;
		Controller->SetInputMode(InputMode);
	}
	if (IsValid(NameInput))
	{
		NameInput->SetUserFocus(GetOwningPlayer());
	}
	
	BP_OnBackClicked();
}

void ULB_CodenameEntryWidget::HandleTextChanged(const FText& Text)
{
	if (bRoomNameMode)
	{
		FString NormalizedRoomName;
		FText ValidationError;
		const bool bLocallyValid = ULB_OnlineSessionSubsystem::ValidateRoomName(
			Text.ToString(), NormalizedRoomName, ValidationError);
		BP_OnTextChanged(Text, bLocallyValid);
		UpdateRoomNameCharacterCount(Text.ToString());
		if (IsValid(ConfirmButton))
		{
			ConfirmButton->SetIsEnabled(bLocallyValid);
		}
		SetErrorText(Text.IsEmpty() || bLocallyValid ? FText::GetEmpty() : ValidationError);
		return;
	}

	if (ALB_MainMenuPlayerController* Controller = Cast<ALB_MainMenuPlayerController>(GetOwningPlayer()))
	{
		Controller->NotifyCodenameDraftChanged(Text);
	}

	FString Sanitized;
	const bool bLocallyValid = ALB_MainMenuGameMode::ValidateCodename(Text.ToString(), Sanitized)
		== ELBCodenameSubmitResult::Accepted;
	if (IsValid(ConfirmButton))
	{
		ConfirmButton->SetIsEnabled(bLocallyValid);
	}
	SetErrorText(FText::GetEmpty());
	BP_OnTextChanged(Text, bLocallyValid);
}

void ULB_CodenameEntryWidget::HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	// Focus loss and clicking elsewhere never submit a network request.
	if (CommitMethod == ETextCommit::OnEnter)
	{
		SubmitCurrentText();
	}
}

void ULB_CodenameEntryWidget::HandleSubmissionResult(
	ELBCodenameSubmitResult Result,
	const FString& SanitizedCodename)
{
	if (Result != ELBCodenameSubmitResult::Accepted)
	{
		SetErrorText(SubmissionErrorText(Result));
		if (IsValid(NameInput))
		{
			NameInput->SetKeyboardFocus();
		}
	}
	BP_OnSubmissionResult(Result);
}

void ULB_CodenameEntryWidget::SubmitCurrentText()
{
	if (!IsValid(NameInput))
	{
		return;
	}

	if (ALB_MainMenuPlayerController* Controller = Cast<ALB_MainMenuPlayerController>(GetOwningPlayer()))
	{
		if (bRoomNameMode)
		{
			Controller->SubmitRoomName(NameInput->GetText());
		}
		else
		{
			Controller->SubmitCodename(NameInput->GetText());
		}
	}
}

void ULB_CodenameEntryWidget::HandleOnlineStateChanged(
	const ELBOnlineState NewState,
	const FText& StatusMessage)
{
	if (!bRoomNameMode)
	{
		return;
	}

	const bool bCreating = NewState == ELBOnlineState::Creating;
	if (IsValid(NameInput))
	{
		NameInput->SetIsReadOnly(bCreating);
	}
	if (IsValid(BackButton))
	{
		BackButton->SetIsEnabled(!bCreating);
	}
	if (IsValid(ConfirmButton))
	{
		FString NormalizedRoomName;
		FText ValidationError;
		const bool bValid = IsValid(NameInput)
			&& ULB_OnlineSessionSubsystem::ValidateRoomName(
				NameInput->GetText().ToString(), NormalizedRoomName, ValidationError);
		ConfirmButton->SetIsEnabled(NewState == ELBOnlineState::Ready && bValid);
	}

	if (bCreating)
	{
		SetErrorText(FText::GetEmpty());
	}
	else if (!StatusMessage.IsEmpty())
	{
		SetErrorText(StatusMessage);
		if (IsValid(NameInput))
		{
			NameInput->SetKeyboardFocus();
		}
	}
	else if (NewState == ELBOnlineState::Error || NewState == ELBOnlineState::SignedOut)
	{
		SetErrorText(LOCTEXT(
			"RoomCreationUnavailable",
			"온라인 연결을 확인한 뒤 다시 시도하세요."));
	}
}

void ULB_CodenameEntryWidget::ConfigureRoomNamePresentation()
{
	if (!WidgetTree)
	{
		return;
	}

	const auto SetText = [this](const FName WidgetName, const FText& Text)
	{
		if (UTextBlock* TextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(WidgetName)))
		{
			TextBlock->SetText(Text);
		}
	};
	SetText(TEXT("TextBlock_31"), LOCTEXT("RoomRegisterLabel", "CREATE ROOM"));
	SetText(
		TEXT("TextBlock_32"),
		LOCTEXT(
			"RoomRegisterDescriptionLine1",
			"탐색대가 함께 모일 방의 이름을 정하세요."));
	SetText(
		TEXT("TextBlock_33"),
		LOCTEXT(
			"RoomRegisterDescriptionLine2",
			"한글·영문·숫자만 사용할 수 있습니다."));
	SetText(
		TEXT("TextBlock_34"),
		LOCTEXT(
			"RoomRegisterDescriptionLine3",
			"공백은 제거되며 같은 이름의 방은 만들 수 없습니다."));
	SetText(TEXT("TXT_Label_2"), LOCTEXT("RoomNameLabel", "방 이름"));
	SetText(TEXT("TXT_Label_3"), LOCTEXT("RoomNameRules", "공백 제외 2~24자"));
	SetText(TEXT("TXT_Back"), LOCTEXT("BackToRoomList", "< 방 목록으로"));

	if (IsValid(NameInput))
	{
		NameInput->SetHintText(LOCTEXT(
			"RoomNameHint", "2~24자, 한글·영문·숫자만 입력하세요"));
	}
	if (UUserWidget* CommonConfirmButton = Cast<UUserWidget>(ConfirmButton))
	{
		if (UTextBlock* ButtonLabel = CommonConfirmButton->WidgetTree
			? Cast<UTextBlock>(CommonConfirmButton->WidgetTree->FindWidget(TEXT("TXT_Label")))
			: nullptr)
		{
			ButtonLabel->SetText(LOCTEXT("CreateRoomButton", "방 만들기"));
		}
	}
	UpdateRoomNameCharacterCount(FString());
}

void ULB_CodenameEntryWidget::UpdateRoomNameCharacterCount(const FString& RawRoomName)
{
	if (!IsValid(CharacterCountText))
	{
		return;
	}

	int32 CharacterCount = 0;
	for (const TCHAR Character : RawRoomName)
	{
		if (!FChar::IsWhitespace(Character))
		{
			++CharacterCount;
		}
	}
	CharacterCountText->SetText(FText::Format(
		LOCTEXT("RoomNameCharacterCount", "{0}/24"),
		FText::AsNumber(CharacterCount)));
}

void ULB_CodenameEntryWidget::SetErrorText(const FText& Message)
{
	if (!IsValid(ErrorText) && WidgetTree)
	{
		ErrorText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LB_CodenameError"));
		ErrorText->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.16f, 0.12f)));
		ErrorText->SetJustification(ETextJustify::Center);

		if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(WidgetTree->RootWidget))
		{
			if (UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(ErrorText))
			{
				CanvasSlot->SetAnchors(FAnchors(0.5f, 0.72f));
				CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
				CanvasSlot->SetAutoSize(true);
			}
		}
		else if (UPanelWidget* Panel = Cast<UPanelWidget>(WidgetTree->RootWidget))
		{
			Panel->AddChild(ErrorText);
		}
	}

	if (IsValid(ErrorText))
	{
		ErrorText->SetText(Message);
		ErrorText->SetVisibility(Message.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

FText ULB_CodenameEntryWidget::SubmissionErrorText(ELBCodenameSubmitResult Result) const
{
	switch (Result)
	{
	case ELBCodenameSubmitResult::TooShort:
		return LOCTEXT("TooShort", "Codename must contain at least 2 characters.");
	case ELBCodenameSubmitResult::TooLong:
		return LOCTEXT("TooLong", "Codename must contain no more than 12 characters.");
	case ELBCodenameSubmitResult::InvalidCharacters:
		return LOCTEXT("InvalidCharacters", "Control characters and line breaks are not allowed.");
	case ELBCodenameSubmitResult::TravelInProgress:
		return LOCTEXT("TravelInProgress", "The raid is already starting.");
	default:
		return LOCTEXT("NotInLobby", "The lobby could not accept this codename.");
	}
}

#undef LOCTEXT_NAMESPACE
