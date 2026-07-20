#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "System/Online/LB_OnlineSessionSubsystem.h"
#include "LB_RoomCreationWidget.generated.h"

class SBorder;
class SButton;
class SEditableTextBox;
class STextBlock;
class SThrobber;
struct FButtonStyle;
struct FSlateBrush;

/** Standalone room-creation screen. It intentionally has no designer-asset dependency. */
UCLASS(Blueprintable)
class LEFTBEHIND_API ULB_RoomCreationWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<ULB_OnlineSessionSubsystem> BoundOnlineSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UObject> TitleFontAsset;

	UPROPERTY(Transient)
	TObjectPtr<UObject> BodyFontAsset;

	TSharedPtr<SEditableTextBox> RoomNameInput;
	TSharedPtr<STextBlock> CharacterCountText;
	TSharedPtr<STextBlock> ValidationText;
	TSharedPtr<STextBlock> PreviewNameText;
	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<SBorder> StatusBanner;
	TSharedPtr<SButton> CreateButton;
	TSharedPtr<SButton> BackButton;
	TSharedPtr<SThrobber> CreatingIndicator;
	TSharedPtr<FButtonStyle> PrimaryButtonStyle;
	TSharedPtr<FButtonStyle> SecondaryButtonStyle;
	TSharedPtr<FSlateBrush> PanelBrush;
	TSharedPtr<FSlateBrush> InputPanelBrush;
	TSharedPtr<FSlateBrush> PreviewBrush;
	TSharedPtr<FSlateBrush> InfoBrush;
	TSharedPtr<FSlateBrush> StatusBrush;

	FString NormalizedDraft;
	bool bDraftValid = false;
	bool bCreating = false;

	void BindOnlineSubsystem();
	void RefreshDraft(const FText& Draft);
	void RefreshControls(ELBOnlineState State);
	void ShowStatus(const FText& Message, bool bIsError);
	void ReturnToRoomBrowser();
	bool CanSubmitDraft() const;

	void HandleRoomNameChanged(const FText& Text);
	void HandleRoomNameCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void HandleOnlineStateChanged(ELBOnlineState NewState, const FText& StatusMessage);

	FReply HandleCreateClicked();
	FReply HandleBackClicked();
};
