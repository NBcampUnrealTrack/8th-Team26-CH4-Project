#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LB_MainMenuRootWidget.generated.h"

class UWidget;

/** Native behavior for the existing WBP_MainMenu designer asset. */
UCLASS(Abstract, Blueprintable)
class LEFTBEHIND_API ULB_MainMenuRootWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UWidget> StartButton;

	UFUNCTION()
	void HandleStartClicked();
};
