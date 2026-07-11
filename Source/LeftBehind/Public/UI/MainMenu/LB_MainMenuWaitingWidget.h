#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "System/MainMenu/LBMainMenuTypes.h"
#include "System/Online/LB_OnlineSessionSubsystem.h"
#include "LB_MainMenuWaitingWidget.generated.h"

class ALB_MainMenuGameState;
class SButton;
class STextBlock;
class SVerticalBox;

/** RepNotify-driven lobby view used as the native parent of WBP_WaitingRoom. */
UCLASS(Abstract, Blueprintable)
class LEFTBEHIND_API ULB_MainMenuWaitingWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<ALB_MainMenuGameState> BoundGameState;

	UPROPERTY(Transient)
	TObjectPtr<ULB_OnlineSessionSubsystem> BoundOnlineSubsystem;

	TSharedPtr<STextBlock> PlayerCountText;
	TSharedPtr<STextBlock> ConfirmedCountText;
	TSharedPtr<STextBlock> TargetMapText;
	TSharedPtr<STextBlock> PhaseText;
	TSharedPtr<STextBlock> OnlineStatusText;
	TSharedPtr<SVerticalBox> PlayerListBox;
	TSharedPtr<SButton> StartButton;
	TSharedPtr<SButton> InviteButton;
	TSharedPtr<SButton> LeaveButton;

	UFUNCTION()
	void HandleSnapshotChanged(const FLBMainMenuSnapshot& Snapshot);

	UFUNCTION()
	void HandleOnlineStateChanged(ELBOnlineState NewState, const FText& StatusMessage);

	void BindGameState();
	void BindOnlineSubsystem();
	void Refresh(const FLBMainMenuSnapshot& Snapshot);
	void RefreshOnlineControls(ELBOnlineState State, const FText& StatusMessage);
	FReply HandleStartClicked();
	FReply HandleInviteClicked();
	FReply HandleLeaveClicked();
};
