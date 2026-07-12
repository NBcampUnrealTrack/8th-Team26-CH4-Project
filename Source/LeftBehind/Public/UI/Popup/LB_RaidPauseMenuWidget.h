#pragma once

#include "CoreMinimal.h"
#include "UI/Base/LB_BaseUserWidget.h"
#include "LB_RaidPauseMenuWidget.generated.h"

class UWidget;

/**
 * Native behaviour for the raid pause action menu.
 *
 * The Widget Blueprint deliberately owns only layout and styling.  All input
 * actions and the three mutually exclusive presentations are controlled here
 * so that recreating the asset cannot duplicate Blueprint event bindings.
 */
UCLASS()
class LEFTBEHIND_API ULB_RaidPauseMenuWidget : public ULB_BaseUserWidget
{
	GENERATED_BODY()

public:
	/** Shows the local action menu. An open quit confirmation is preserved. */
	UFUNCTION(BlueprintCallable, Category = "LB|Raid|Pause Menu")
	void ShowActionMenu(bool bCanReturnToRoom);

	/** Shows the read-only notice used when the listen host paused the raid. */
	UFUNCTION(BlueprintCallable, Category = "LB|Raid|Pause Menu")
	void ShowHostPauseNotice();

	/** Collapses the complete overlay without removing it from the viewport. */
	UFUNCTION(BlueprintCallable, Category = "LB|Raid|Pause Menu")
	void HidePauseOverlay();

	/** Updates host-only room-return availability without changing presentation. */
	UFUNCTION(BlueprintCallable, Category = "LB|Raid|Pause Menu")
	void SetCanReturnToRoom(bool bCanReturnToRoom);

	UFUNCTION(BlueprintPure, Category = "LB|Raid|Pause Menu")
	bool IsQuitConfirmationVisible() const;

	/** Compatibility spelling used by the controller's Escape handling. */
	UFUNCTION(BlueprintPure, Category = "LB|Raid|Pause Menu")
	bool IsShowingQuitConfirmation() const { return IsQuitConfirmationVisible(); }

	UFUNCTION(BlueprintCallable, Category = "LB|Raid|Pause Menu")
	void CancelQuitConfirmation();

	/** Returns a safe target for FInputModeGameAndUI focus. */
	UFUNCTION(BlueprintPure, Category = "LB|Raid|Pause Menu")
	UWidget* GetPreferredFocusTarget() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UWidget> ResumeButton;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> ReturnToRoomButton;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> QuitButton;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> ConfirmQuitButton;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> CancelQuitButton;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> ActionMenuPanel;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> QuitConfirmPanel;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> HostPauseNoticeText;

	bool bCanReturnToRoom = false;

	UFUNCTION()
	void HandleResumeClicked();

	UFUNCTION()
	void HandleReturnToRoomClicked();

	UFUNCTION()
	void HandleQuitClicked();

	UFUNCTION()
	void HandleConfirmQuitClicked();

	UFUNCTION()
	void HandleCancelQuitClicked();

	void CacheContractWidgets();
	void BindContractButtons();
	void UnbindContractButtons();
	void ShowQuitConfirmation();
	void RefreshReturnToRoomButton();
};
