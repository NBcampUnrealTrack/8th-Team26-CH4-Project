#include "UI/Popup/LB_RaidPauseMenuWidget.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "InputCoreTypes.h"
#include "Player/LB_PlayerController.h"
#include "UObject/UnrealType.h"

namespace
{
	FMulticastDelegateProperty* FindPauseMenuClickDelegate(const UWidget* Widget)
	{
		if (!IsValid(Widget))
		{
			return nullptr;
		}

		if (FMulticastDelegateProperty* CustomDelegate = FindFProperty<FMulticastDelegateProperty>(
			Widget->GetClass(), TEXT("OnBTNClicked")))
		{
			return CustomDelegate;
		}

		return FindFProperty<FMulticastDelegateProperty>(Widget->GetClass(), TEXT("OnClicked"));
	}

	void BindPauseMenuClick(UWidget* Widget, UObject* Handler, const FName FunctionName)
	{
		if (FMulticastDelegateProperty* Property = FindPauseMenuClickDelegate(Widget))
		{
			void* Value = Property->ContainerPtrToValuePtr<void>(Widget);
			FScriptDelegate Delegate;
			Delegate.BindUFunction(Handler, FunctionName);
			Property->RemoveDelegate(Delegate, Widget, Value);
			Property->AddDelegate(Delegate, Widget, Value);
		}
	}

	void UnbindPauseMenuClick(UWidget* Widget, UObject* Handler, const FName FunctionName)
	{
		if (FMulticastDelegateProperty* Property = FindPauseMenuClickDelegate(Widget))
		{
			FScriptDelegate Delegate;
			Delegate.BindUFunction(Handler, FunctionName);
			Property->RemoveDelegate(
				Delegate,
				Widget,
				Property->ContainerPtrToValuePtr<void>(Widget));
		}
	}

	void SetContractVisibility(UWidget* Widget, const ESlateVisibility Visibility)
	{
		if (IsValid(Widget))
		{
			Widget->SetVisibility(Visibility);
		}
	}

	UWidget* ResolvePauseMenuFocusTarget(UWidget* ContractButton)
	{
		UUserWidget* CommonButton = Cast<UUserWidget>(ContractButton);
		if (IsValid(CommonButton))
		{
			// The shared button's inner UButton is intentionally mouse-only. Make
			// this menu's wrapper instances focusable and translate Accept keys in
			// NativeOnKeyDown so keyboard/gamepad focus never targets that inner BTN.
			CommonButton->SetIsFocusable(true);
			return CommonButton;
		}

		return ContractButton;
	}
}

void ULB_RaidPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	CacheContractWidgets();
	BindContractButtons();
	RefreshReturnToRoomButton();
}

void ULB_RaidPauseMenuWidget::NativeDestruct()
{
	UnbindContractButtons();

	ResumeButton = nullptr;
	ReturnToRoomButton = nullptr;
	QuitButton = nullptr;
	ConfirmQuitButton = nullptr;
	CancelQuitButton = nullptr;
	ActionMenuPanel = nullptr;
	QuitConfirmPanel = nullptr;
	HostPauseNoticeText = nullptr;

	Super::NativeDestruct();
}

FReply ULB_RaidPauseMenuWidget::NativeOnKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	const bool bAcceptPressed = Key == EKeys::Enter
		|| Key == EKeys::SpaceBar
		|| Key == EKeys::Virtual_Gamepad_Accept.GetVirtualKey()
		|| Key == EKeys::Gamepad_FaceButton_Bottom;
	if (!bAcceptPressed)
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}

	if (IsQuitConfirmationVisible())
	{
		if (IsValid(ConfirmQuitButton) && ConfirmQuitButton->HasAnyUserFocus())
		{
			HandleConfirmQuitClicked();
			return FReply::Handled();
		}
		if (IsValid(CancelQuitButton) && CancelQuitButton->HasAnyUserFocus())
		{
			HandleCancelQuitClicked();
			return FReply::Handled();
		}
	}
	else
	{
		if (IsValid(ResumeButton) && ResumeButton->HasAnyUserFocus())
		{
			HandleResumeClicked();
			return FReply::Handled();
		}
		if (IsValid(ReturnToRoomButton) && ReturnToRoomButton->HasAnyUserFocus())
		{
			HandleReturnToRoomClicked();
			return FReply::Handled();
		}
		if (IsValid(QuitButton) && QuitButton->HasAnyUserFocus())
		{
			HandleQuitClicked();
			return FReply::Handled();
		}
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void ULB_RaidPauseMenuWidget::ShowActionMenu(const bool bInCanReturnToRoom)
{
	SetCanReturnToRoom(bInCanReturnToRoom);
	SetVisibility(ESlateVisibility::Visible);

	// A host-pause replication refresh must not throw the user out of the quit
	// confirmation they are already interacting with.
	if (IsQuitConfirmationVisible())
	{
		return;
	}

	SetContractVisibility(ActionMenuPanel, ESlateVisibility::Visible);
	SetContractVisibility(QuitConfirmPanel, ESlateVisibility::Collapsed);
	SetContractVisibility(HostPauseNoticeText, ESlateVisibility::Collapsed);

	if (IsValid(ResumeButton))
	{
		ResumeButton->SetIsEnabled(true);
	}
	if (IsValid(QuitButton))
	{
		QuitButton->SetIsEnabled(true);
	}

	if (UWidget* FocusTarget = GetPreferredFocusTarget())
	{
		FocusTarget->SetKeyboardFocus();
	}
}

void ULB_RaidPauseMenuWidget::ShowHostPauseNotice()
{
	SetVisibility(ESlateVisibility::Visible);
	SetContractVisibility(ActionMenuPanel, ESlateVisibility::Collapsed);
	SetContractVisibility(QuitConfirmPanel, ESlateVisibility::Collapsed);
	SetContractVisibility(HostPauseNoticeText, ESlateVisibility::Visible);
}

void ULB_RaidPauseMenuWidget::HidePauseOverlay()
{
	SetContractVisibility(ActionMenuPanel, ESlateVisibility::Collapsed);
	SetContractVisibility(QuitConfirmPanel, ESlateVisibility::Collapsed);
	SetContractVisibility(HostPauseNoticeText, ESlateVisibility::Collapsed);
	SetVisibility(ESlateVisibility::Collapsed);

	if (IsValid(ConfirmQuitButton))
	{
		ConfirmQuitButton->SetIsEnabled(true);
	}
}

void ULB_RaidPauseMenuWidget::SetCanReturnToRoom(const bool bInCanReturnToRoom)
{
	bCanReturnToRoom = bInCanReturnToRoom;
	RefreshReturnToRoomButton();
}

bool ULB_RaidPauseMenuWidget::IsQuitConfirmationVisible() const
{
	return IsVisible() && IsValid(QuitConfirmPanel) && QuitConfirmPanel->IsVisible();
}

void ULB_RaidPauseMenuWidget::CancelQuitConfirmation()
{
	if (IsValid(ConfirmQuitButton))
	{
		ConfirmQuitButton->SetIsEnabled(true);
	}

	SetContractVisibility(QuitConfirmPanel, ESlateVisibility::Collapsed);
	ShowActionMenu(bCanReturnToRoom);
}

UWidget* ULB_RaidPauseMenuWidget::GetPreferredFocusTarget() const
{
	if (IsQuitConfirmationVisible())
	{
		return ResolvePauseMenuFocusTarget(
			IsValid(CancelQuitButton) ? CancelQuitButton.Get() : ConfirmQuitButton.Get());
	}

	if (IsVisible() && IsValid(ActionMenuPanel) && ActionMenuPanel->IsVisible())
	{
		return ResolvePauseMenuFocusTarget(
			IsValid(ResumeButton) ? ResumeButton.Get() : QuitButton.Get());
	}

	return nullptr;
}

void ULB_RaidPauseMenuWidget::HandleResumeClicked()
{
	if (ALB_PlayerController* Controller = Cast<ALB_PlayerController>(GetOwningPlayer()))
	{
		Controller->ClosePauseMenu();
	}
}

void ULB_RaidPauseMenuWidget::HandleReturnToRoomClicked()
{
	if (!IsValid(ReturnToRoomButton) || !bCanReturnToRoom)
	{
		return;
	}

	ReturnToRoomButton->SetIsEnabled(false);
	ALB_PlayerController* Controller = Cast<ALB_PlayerController>(GetOwningPlayer());
	if (!IsValid(Controller) || !Controller->RequestAbortRaidToRoom())
	{
		RefreshReturnToRoomButton();
	}
}

void ULB_RaidPauseMenuWidget::HandleQuitClicked()
{
	ShowQuitConfirmation();
}

void ULB_RaidPauseMenuWidget::HandleConfirmQuitClicked()
{
	if (IsValid(ConfirmQuitButton))
	{
		ConfirmQuitButton->SetIsEnabled(false);
	}

	if (ALB_PlayerController* Controller = Cast<ALB_PlayerController>(GetOwningPlayer()))
	{
		Controller->ConfirmQuitGame();
		return;
	}

	if (IsValid(ConfirmQuitButton))
	{
		ConfirmQuitButton->SetIsEnabled(true);
	}
}

void ULB_RaidPauseMenuWidget::HandleCancelQuitClicked()
{
	CancelQuitConfirmation();
}

void ULB_RaidPauseMenuWidget::CacheContractWidgets()
{
	ResumeButton = WidgetTree ? WidgetTree->FindWidget(TEXT("BTN_Resume")) : nullptr;
	ReturnToRoomButton = WidgetTree ? WidgetTree->FindWidget(TEXT("BTN_ReturnToRoom")) : nullptr;
	QuitButton = WidgetTree ? WidgetTree->FindWidget(TEXT("BTN_Quit")) : nullptr;
	ConfirmQuitButton = WidgetTree ? WidgetTree->FindWidget(TEXT("BTN_ConfirmQuit")) : nullptr;
	CancelQuitButton = WidgetTree ? WidgetTree->FindWidget(TEXT("BTN_CancelQuit")) : nullptr;
	ActionMenuPanel = WidgetTree ? WidgetTree->FindWidget(TEXT("PNL_ActionMenu")) : nullptr;
	QuitConfirmPanel = WidgetTree ? WidgetTree->FindWidget(TEXT("PNL_QuitConfirm")) : nullptr;
	HostPauseNoticeText = WidgetTree ? WidgetTree->FindWidget(TEXT("TXT_HostPauseNotice")) : nullptr;

	ResolvePauseMenuFocusTarget(ResumeButton);
	ResolvePauseMenuFocusTarget(ReturnToRoomButton);
	ResolvePauseMenuFocusTarget(QuitButton);
	ResolvePauseMenuFocusTarget(ConfirmQuitButton);
	ResolvePauseMenuFocusTarget(CancelQuitButton);
}

void ULB_RaidPauseMenuWidget::BindContractButtons()
{
	BindPauseMenuClick(ResumeButton, this, GET_FUNCTION_NAME_CHECKED(ThisClass, HandleResumeClicked));
	BindPauseMenuClick(ReturnToRoomButton, this, GET_FUNCTION_NAME_CHECKED(ThisClass, HandleReturnToRoomClicked));
	BindPauseMenuClick(QuitButton, this, GET_FUNCTION_NAME_CHECKED(ThisClass, HandleQuitClicked));
	BindPauseMenuClick(ConfirmQuitButton, this, GET_FUNCTION_NAME_CHECKED(ThisClass, HandleConfirmQuitClicked));
	BindPauseMenuClick(CancelQuitButton, this, GET_FUNCTION_NAME_CHECKED(ThisClass, HandleCancelQuitClicked));
}

void ULB_RaidPauseMenuWidget::UnbindContractButtons()
{
	UnbindPauseMenuClick(ResumeButton, this, GET_FUNCTION_NAME_CHECKED(ThisClass, HandleResumeClicked));
	UnbindPauseMenuClick(ReturnToRoomButton, this, GET_FUNCTION_NAME_CHECKED(ThisClass, HandleReturnToRoomClicked));
	UnbindPauseMenuClick(QuitButton, this, GET_FUNCTION_NAME_CHECKED(ThisClass, HandleQuitClicked));
	UnbindPauseMenuClick(ConfirmQuitButton, this, GET_FUNCTION_NAME_CHECKED(ThisClass, HandleConfirmQuitClicked));
	UnbindPauseMenuClick(CancelQuitButton, this, GET_FUNCTION_NAME_CHECKED(ThisClass, HandleCancelQuitClicked));
}

void ULB_RaidPauseMenuWidget::ShowQuitConfirmation()
{
	SetVisibility(ESlateVisibility::Visible);
	SetContractVisibility(ActionMenuPanel, ESlateVisibility::Collapsed);
	SetContractVisibility(QuitConfirmPanel, ESlateVisibility::Visible);
	SetContractVisibility(HostPauseNoticeText, ESlateVisibility::Collapsed);

	if (IsValid(ConfirmQuitButton))
	{
		ConfirmQuitButton->SetIsEnabled(true);
	}
	if (IsValid(CancelQuitButton))
	{
		CancelQuitButton->SetIsEnabled(true);
	}

	if (UWidget* FocusTarget = GetPreferredFocusTarget())
	{
		FocusTarget->SetKeyboardFocus();
	}
}

void ULB_RaidPauseMenuWidget::RefreshReturnToRoomButton()
{
	if (!IsValid(ReturnToRoomButton))
	{
		return;
	}

	ReturnToRoomButton->SetVisibility(
		bCanReturnToRoom ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	ReturnToRoomButton->SetIsEnabled(bCanReturnToRoom);
}
