#include "LBMainMenuPIETestBridge.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/EditableText.h"
#include "Components/Widget.h"
#include "Engine/World.h"
#include "Player/LB_MainMenuPlayerController.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

namespace
{
	bool BroadcastWidgetClick(UUserWidget* UserWidget, FName ButtonWidgetName)
	{
		UWidget* ButtonWidget = IsValid(UserWidget) && UserWidget->WidgetTree
			? UserWidget->WidgetTree->FindWidget(ButtonWidgetName)
			: nullptr;
		if (!IsValid(ButtonWidget))
		{
			return false;
		}

		FMulticastDelegateProperty* ClickProperty =
			FindFProperty<FMulticastDelegateProperty>(ButtonWidget->GetClass(), TEXT("OnBTNClicked"));
		if (!ClickProperty)
		{
			ClickProperty = FindFProperty<FMulticastDelegateProperty>(ButtonWidget->GetClass(), TEXT("OnClicked"));
		}
		if (!ClickProperty)
		{
			return false;
		}

		void* DelegateValue = ClickProperty->ContainerPtrToValuePtr<void>(ButtonWidget);
		const FMulticastScriptDelegate* Delegate = ClickProperty->GetMulticastDelegate(DelegateValue);
		if (!Delegate || !Delegate->IsBound())
		{
			return false;
		}
		Delegate->ProcessMulticastDelegate<UObject>(nullptr);
		return true;
	}
}

bool ULBMainMenuPIETestBridge::ScheduleCodenameSubmit(
	ALB_MainMenuPlayerController* Controller,
	const FString& Codename)
{
	if (!IsValid(Controller) || !IsValid(Controller->GetWorld()))
	{
		return false;
	}

	const TWeakObjectPtr<ALB_MainMenuPlayerController> WeakController(Controller);
	Controller->GetWorld()->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateLambda([WeakController, Codename]()
		{
			if (ALB_MainMenuPlayerController* StrongController = WeakController.Get())
			{
				StrongController->SubmitCodename(FText::FromString(Codename));
			}
		}));
	return true;
}

bool ULBMainMenuPIETestBridge::ScheduleStartRequests(
	ALB_MainMenuPlayerController* Controller,
	int32 RequestCount)
{
	if (!IsValid(Controller) || !IsValid(Controller->GetWorld()) || RequestCount <= 0)
	{
		return false;
	}

	const TWeakObjectPtr<ALB_MainMenuPlayerController> WeakController(Controller);
	Controller->GetWorld()->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateLambda([WeakController, RequestCount]()
		{
			if (ALB_MainMenuPlayerController* StrongController = WeakController.Get())
			{
				for (int32 Index = 0; Index < RequestCount; ++Index)
				{
					StrongController->RequestStartCharacterSelect();
				}
			}
		}));
	return true;
}

bool ULBMainMenuPIETestBridge::ScheduleLobbyReady(
	ALB_MainMenuPlayerController* Controller,
	const bool bReady)
{
	if (!IsValid(Controller) || !IsValid(Controller->GetWorld()))
	{
		return false;
	}

	const TWeakObjectPtr<ALB_MainMenuPlayerController> WeakController(Controller);
	Controller->GetWorld()->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateLambda([WeakController, bReady]()
		{
			if (ALB_MainMenuPlayerController* StrongController = WeakController.Get())
			{
				StrongController->RequestSetLobbyReady(bReady);
			}
		}));
	return true;
}

bool ULBMainMenuPIETestBridge::ScheduleWidgetClick(UUserWidget* Widget, FName ButtonWidgetName)
{
	if (!IsValid(Widget) || !IsValid(Widget->GetWorld()))
	{
		return false;
	}

	const TWeakObjectPtr<UUserWidget> WeakWidget(Widget);
	Widget->GetWorld()->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateLambda([WeakWidget, ButtonWidgetName]()
		{
			if (UUserWidget* StrongWidget = WeakWidget.Get())
			{
				ensureAlwaysMsgf(
					BroadcastWidgetClick(StrongWidget, ButtonWidgetName),
					TEXT("PIE test could not broadcast %s.%s"),
					*StrongWidget->GetName(),
					*ButtonWidgetName.ToString());
			}
		}));
	return true;
}

bool ULBMainMenuPIETestBridge::ScheduleCodenameWidgetSubmit(UUserWidget* Widget, const FString& Codename)
{
	if (!IsValid(Widget) || !IsValid(Widget->GetWorld()))
	{
		return false;
	}

	const TWeakObjectPtr<UUserWidget> WeakWidget(Widget);
	Widget->GetWorld()->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateLambda([WeakWidget, Codename]()
		{
			UUserWidget* StrongWidget = WeakWidget.Get();
			UEditableText* NameInput = IsValid(StrongWidget) && StrongWidget->WidgetTree
				? Cast<UEditableText>(StrongWidget->WidgetTree->FindWidget(TEXT("ETB_Name")))
				: nullptr;
			if (ensureAlwaysMsgf(IsValid(NameInput), TEXT("PIE test could not find ETB_Name")))
			{
				NameInput->SetText(FText::FromString(Codename));
				ensureAlwaysMsgf(
					BroadcastWidgetClick(StrongWidget, TEXT("BTN_Confirm")),
					TEXT("PIE test could not broadcast BTN_Confirm"));
			}
		}));
	return true;
}
