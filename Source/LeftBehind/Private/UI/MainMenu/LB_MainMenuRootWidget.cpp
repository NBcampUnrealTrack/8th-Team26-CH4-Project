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
	if (IsValid(StartButton))
	{
		BindMainMenuRootClick(StartButton, this, GET_FUNCTION_NAME_CHECKED(ThisClass, HandleStartClicked));
		StartButton->SetIsEnabled(true);
	}
}

void ULB_MainMenuRootWidget::NativeDestruct()
{
	if (IsValid(StartButton))
	{
		UnbindMainMenuRootClick(StartButton, this, GET_FUNCTION_NAME_CHECKED(ThisClass, HandleStartClicked));
	}
	StartButton = nullptr;
	Super::NativeDestruct();
}

void ULB_MainMenuRootWidget::HandleStartClicked()
{
	if (ALB_MainMenuPlayerController* Controller = Cast<ALB_MainMenuPlayerController>(GetOwningPlayer()))
	{
		Controller->BeginOnlinePlay();
	}
}
