// LB_RaidResultWidget.cpp

#include "UI/Popup/LB_RaidResultWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Player/LB_PlayerController.h"
#include "UObject/UnrealType.h"

namespace
{
	FMulticastDelegateProperty* FindRaidResultClickDelegate(UWidget* Widget)
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

	void BindRaidResultClick(UWidget* Widget, UObject* Handler, FName FunctionName)
	{
		if (FMulticastDelegateProperty* Property = FindRaidResultClickDelegate(Widget))
		{
			void* Value = Property->ContainerPtrToValuePtr<void>(Widget);
			FScriptDelegate Delegate;
			Delegate.BindUFunction(Handler, FunctionName);
			Property->RemoveDelegate(Delegate, Widget, Value);
			Property->AddDelegate(Delegate, Widget, Value);
		}
	}

	void UnbindRaidResultClick(UWidget* Widget, UObject* Handler, FName FunctionName)
	{
		if (FMulticastDelegateProperty* Property = FindRaidResultClickDelegate(Widget))
		{
			FScriptDelegate Delegate;
			Delegate.BindUFunction(Handler, FunctionName);
			Property->RemoveDelegate(
				Delegate,
				Widget,
				Property->ContainerPtrToValuePtr<void>(Widget));
		}
	}
}

void ULB_RaidResultWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ReturnToMainMenuButton = WidgetTree
		? WidgetTree->FindWidget(TEXT("BTN_ReturnToMainMenu"))
		: nullptr;

	if (IsValid(ReturnToMainMenuButton))
	{
		BindRaidResultClick(
			ReturnToMainMenuButton,
			this,
			GET_FUNCTION_NAME_CHECKED(ThisClass, HandleReturnToMainMenuClicked));
	}

	RefreshReturnToMainMenuButton();
}

void ULB_RaidResultWidget::NativeDestruct()
{
	if (IsValid(ReturnToMainMenuButton))
	{
		UnbindRaidResultClick(
			ReturnToMainMenuButton,
			this,
			GET_FUNCTION_NAME_CHECKED(ThisClass, HandleReturnToMainMenuClicked));
	}

	ReturnToMainMenuButton = nullptr;
	Super::NativeDestruct();
}

void ULB_RaidResultWidget::HandleRaidStateChanged(ELBRaidState NewState)
{
	Super::HandleRaidStateChanged(NewState);

	CurrentRaidState = NewState;
	RefreshReturnToMainMenuButton();
}

void ULB_RaidResultWidget::HandleRaidResultChanged(const FLBRaidResultData& ResultData)
{
	Super::HandleRaidResultChanged(ResultData);

	BP_UpdateResult(ResultData);
}

void ULB_RaidResultWidget::HandleReturnToMainMenuClicked()
{
	if (!IsValid(ReturnToMainMenuButton))
	{
		return;
	}

	// Block rapid repeat clicks before attempting travel. A rejected request restores
	// the state from the authoritative controller/game mode checks below.
	ReturnToMainMenuButton->SetIsEnabled(false);

	ALB_PlayerController* Controller = Cast<ALB_PlayerController>(GetOwningPlayer());
	if (!IsValid(Controller) || !Controller->RequestReturnToMainMenu())
	{
		RefreshReturnToMainMenuButton();
	}
}

void ULB_RaidResultWidget::RefreshReturnToMainMenuButton()
{
	if (!IsValid(ReturnToMainMenuButton))
	{
		return;
	}

	ALB_PlayerController* Controller = Cast<ALB_PlayerController>(GetOwningPlayer());
	const bool bCanShow = IsValid(Controller) && Controller->IsLocalListenHost();
	ReturnToMainMenuButton->SetVisibility(
		bCanShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	const bool bCanRequest = bCanShow
		&& CurrentRaidState == ELBRaidState::Result
		&& Controller->CanRequestReturnToMainMenu();
	ReturnToMainMenuButton->SetIsEnabled(bCanRequest);
}
