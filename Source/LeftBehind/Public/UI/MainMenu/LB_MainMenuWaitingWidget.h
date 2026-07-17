#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "System/MainMenu/LBMainMenuTypes.h"
#include "System/Online/LB_OnlineSessionSubsystem.h"
#include "LB_MainMenuWaitingWidget.generated.h"

class ALB_MainMenuGameState;
class SBorder;
class SButton;
class SProgressBar;
class STextBlock;
class SVerticalBox;
struct FButtonStyle;
struct FSlateBrush;

/** RepNotify-driven lobby view used as the native parent of WBP_WaitingRoom. */
UCLASS(Abstract, Blueprintable)
class LEFTBEHIND_API ULB_MainMenuWaitingWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<ALB_MainMenuGameState> BoundGameState;

	UPROPERTY(Transient)
	TObjectPtr<ULB_OnlineSessionSubsystem> BoundOnlineSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UObject> TitleFontAsset;

	UPROPERTY(Transient)
	TObjectPtr<UObject> BodyFontAsset;

	UPROPERTY(Transient)
	TObjectPtr<UObject> HudNumberFontAsset;

	TSharedPtr<STextBlock> RoomNameText;
	TSharedPtr<STextBlock> RoomRoleText;
	TSharedPtr<STextBlock> PlayerCountText;
	TSharedPtr<STextBlock> ConfirmedCountText;
	TSharedPtr<STextBlock> TargetMapText;
	TSharedPtr<STextBlock> PhaseText;
	TSharedPtr<STextBlock> ReadyPercentText;
	TSharedPtr<STextBlock> OnlineStatusText;
	TSharedPtr<SBorder> OnlineStatusBorder;
	TSharedPtr<SProgressBar> ReadyProgressBar;
	TSharedPtr<SVerticalBox> PlayerListBox;
	TSharedPtr<SButton> StartButton;
	TSharedPtr<SButton> InviteButton;
	TSharedPtr<SButton> LeaveButton;
	TSharedPtr<FButtonStyle> PrimaryButtonStyle;
	TSharedPtr<FButtonStyle> SecondaryButtonStyle;
	TSharedPtr<FButtonStyle> TextButtonStyle;
	TSharedPtr<FSlateBrush> MainPanelBrush;
	TSharedPtr<FSlateBrush> CardBrush;
	TSharedPtr<FSlateBrush> RosterRowBrush;
	TSharedPtr<FSlateBrush> WarningBrush;

	UFUNCTION()
	void HandleSnapshotChanged(const FLBMainMenuSnapshot& Snapshot);

	UFUNCTION()
	void HandleOnlineStateChanged(ELBOnlineState NewState, const FText& StatusMessage);

	void BindGameState();
	void BindOnlineSubsystem();
	void RefreshRoomIdentity();
	void Refresh(const FLBMainMenuSnapshot& Snapshot);
	void RefreshOnlineControls(ELBOnlineState State, const FText& StatusMessage);
	FReply HandleStartClicked();
	FReply HandleInviteClicked();
	FReply HandleLeaveClicked();
};
