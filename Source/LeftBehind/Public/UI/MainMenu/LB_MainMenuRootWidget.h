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

public:
	/** Opens the existing Start/room-entry panel in WBP_MainMenu. */
	void ShowRoomEntryPanel();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category="LB|UI")
	void BP_OnStartClicked();

	UFUNCTION(BlueprintCallable, Category="LB|UI")
	void ExecuteOnlinePlay();
	
	UFUNCTION(BlueprintCallable, Category="LB|UI")
	void ResetOnlinePlayFlag();
	
private:
	UPROPERTY(Transient)
	TObjectPtr<UWidget> StartButton;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> BackButton;

	UFUNCTION()
	void HandleStartClicked();

	UFUNCTION()
	void HandleBackClicked();

	bool RunBlueprintTransition(FName FunctionName);
	void ShowRootPanelFallback();
	void ShowRoomEntryPanelFallback();
	
	bool bOnlinePlayRequested = false;
};
