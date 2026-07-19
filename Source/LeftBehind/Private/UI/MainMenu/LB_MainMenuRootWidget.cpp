#include "UI/MainMenu/LB_MainMenuRootWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Player/LB_MainMenuPlayerController.h"
#include "UObject/UnrealType.h"

namespace
{
	FMulticastDelegateProperty* FindMainMenuRootClickDelegate(UWidget* Widget)
	{
		if (!IsValid(Widget))
		{
			return nullptr;
		}
		if (FMulticastDelegateProperty* BackDelegate = FindFProperty<FMulticastDelegateProperty>(Widget->GetClass(), TEXT("OnBackBTNPressed")))
		{
			return BackDelegate;
		}
		if (FMulticastDelegateProperty* CustomDelegate = FindFProperty<FMulticastDelegateProperty>(Widget->GetClass(), TEXT("OnBTNClicked")))
		{
			return CustomDelegate;
		}
		return FindFProperty<FMulticastDelegateProperty>(Widget->GetClass(), TEXT("OnClicked"));
	}

	void BindMainMenuRootClick(UWidget* Widget, UObject* Handler, FName FunctionName)
	{
		if (FMulticastDelegateProperty* Property = FindMainMenuRootClickDelegate(Widget))
		{
			void* Value = Property->ContainerPtrToValuePtr<void>(Widget);
			Property->ClearDelegate(Widget, Value);
			FScriptDelegate Delegate;
			Delegate.BindUFunction(Handler, FunctionName);
			Property->AddDelegate(Delegate, Widget, Value);
		}
	}

	void UnbindMainMenuRootClick(UWidget* Widget, UObject* Handler, FName FunctionName)
	{
		if (FMulticastDelegateProperty* Property = FindMainMenuRootClickDelegate(Widget))
		{
			FScriptDelegate Delegate;
			Delegate.BindUFunction(Handler, FunctionName);
			Property->RemoveDelegate(Delegate, Widget, Property->ContainerPtrToValuePtr<void>(Widget));
		}
	}
}

void ULB_MainMenuRootWidget::NativeConstruct()
{
	Super::NativeConstruct();

	StartButton = WidgetTree ? WidgetTree->FindWidget(TEXT("BTN_Start_Start")) : nullptr;
	BackButton = WidgetTree ? WidgetTree->FindWidget(TEXT("BTN_BackBtn")) : nullptr;
	if (IsValid(StartButton))
	{
		BindMainMenuRootClick(StartButton, this, GET_FUNCTION_NAME_CHECKED(ThisClass, HandleStartClicked));
		StartButton->SetIsEnabled(true);
	}
	if (IsValid(BackButton))
	{
		// Keep one reliable native binding while allowing the Blueprint to own its
		// history and transition animations.
		BindMainMenuRootClick(BackButton, this, GET_FUNCTION_NAME_CHECKED(ThisClass, HandleBackClicked));
		BackButton->SetIsEnabled(true);
	}
}

void ULB_MainMenuRootWidget::NativeDestruct()
{
	if (IsValid(StartButton))
	{
		UnbindMainMenuRootClick(StartButton, this, GET_FUNCTION_NAME_CHECKED(ThisClass, HandleStartClicked));
	}
	if (IsValid(BackButton))
	{
		UnbindMainMenuRootClick(BackButton, this, GET_FUNCTION_NAME_CHECKED(ThisClass, HandleBackClicked));
	}
	StartButton = nullptr;
	BackButton = nullptr;
	Super::NativeDestruct();
}

void ULB_MainMenuRootWidget::ExecuteOnlinePlay()
{
	if (bOnlinePlayRequested) return;
	bOnlinePlayRequested = true;
	
	if (ALB_MainMenuPlayerController* Controller = Cast<ALB_MainMenuPlayerController>(GetOwningPlayer()))
	{
		Controller->BeginOnlinePlay();
	}
}

void ULB_MainMenuRootWidget::ResetOnlinePlayFlag()
{
	bOnlinePlayRequested = false;
}

void ULB_MainMenuRootWidget::HandleStartClicked()
{
	BP_OnStartClicked();
}

void ULB_MainMenuRootWidget::HandleBackClicked()
{
	if (!RunBlueprintTransition(TEXT("GoBack")))
	{
		ShowRootPanelFallback();
	}
}

void ULB_MainMenuRootWidget::ShowRoomEntryPanel()
{
	ResetOnlinePlayFlag();
	if (!RunBlueprintTransition(TEXT("BackToStartPanel")))
	{
		ShowRoomEntryPanelFallback();
	}
}

bool ULB_MainMenuRootWidget::RunBlueprintTransition(const FName FunctionName)
{
	UFunction* TransitionFunction = FindFunction(FunctionName);
	if (!IsValid(TransitionFunction) || TransitionFunction->ParmsSize != 0)
	{
		return false;
	}

	ProcessEvent(TransitionFunction, nullptr);
	return true;
}

void ULB_MainMenuRootWidget::ShowRootPanelFallback()
{
	if (IsValid(BackButton))
	{
		BackButton->SetVisibility(ESlateVisibility::Hidden);
	}

	static const FName SubmenuPanelNames[] = {
		TEXT("SB_StartPanel"),
		TEXT("SB_RecordPanel"),
		TEXT("SB_RosterPanel"),
		TEXT("SB_SettingsPanel"),
		TEXT("SB_ExitPanel")
	};
	if (WidgetTree)
	{
		for (const FName PanelName : SubmenuPanelNames)
		{
			if (UWidget* Panel = WidgetTree->FindWidget(PanelName))
			{
				Panel->SetVisibility(ESlateVisibility::Hidden);
			}
		}
	}
	if (UWidget* RootPanel = WidgetTree ? WidgetTree->FindWidget(TEXT("SB_RootMenu")) : nullptr)
	{
		RootPanel->SetVisibility(ESlateVisibility::Visible);
	}
}

void ULB_MainMenuRootWidget::ShowRoomEntryPanelFallback()
{
	if (UWidget* RootPanel = WidgetTree ? WidgetTree->FindWidget(TEXT("SB_RootMenu")) : nullptr)
	{
		RootPanel->SetVisibility(ESlateVisibility::Hidden);
	}
	if (UWidget* StartPanel = WidgetTree ? WidgetTree->FindWidget(TEXT("SB_StartPanel")) : nullptr)
	{
		StartPanel->SetVisibility(ESlateVisibility::Visible);
	}
	if (IsValid(BackButton))
	{
		BackButton->SetVisibility(ESlateVisibility::Visible);
	}
}
