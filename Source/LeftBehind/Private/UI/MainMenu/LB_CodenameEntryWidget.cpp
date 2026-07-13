#include "UI/MainMenu/LB_CodenameEntryWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableText.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "GameMode/LB_MainMenuGameMode.h"
#include "Player/LB_MainMenuPlayerController.h"
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
		HandleTextChanged(NameInput->GetText());
		NameInput->SetKeyboardFocus();
	}

	if (ALB_MainMenuPlayerController* Controller = Cast<ALB_MainMenuPlayerController>(GetOwningPlayer()))
	{
		Controller->OnCodenameSubmissionResult.AddUniqueDynamic(this, &ThisClass::HandleSubmissionResult);
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

	ConfirmButton = nullptr;
	BackButton = nullptr;
	NameInput = nullptr;
	ErrorText = nullptr;
	Super::NativeDestruct();
}

void ULB_CodenameEntryWidget::HandleConfirmClicked()
{
	BP_OnConfirmClicked();
	//SubmitCurrentText();
}

void ULB_CodenameEntryWidget::HandleBackClicked()
{
	if (IsValid(NameInput))
	{
		NameInput->SetUserFocus(GetOwningPlayer());
	}
	
	if (ALB_MainMenuPlayerController* Controller = Cast<ALB_MainMenuPlayerController>(GetOwningPlayer()))
	{
		FInputModeGameAndUI InputMode;
		Controller->SetInputMode(InputMode);
		//Controller->SetMenuScreen(ELBMainMenuScreen::Main);
	}
	
	BP_OnBackClicked();
}

void ULB_CodenameEntryWidget::HandleTextChanged(const FText& Text)
{
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
		Controller->SubmitCodename(NameInput->GetText());
	}
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
