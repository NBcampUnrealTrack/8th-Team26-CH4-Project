#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "System/Online/LB_OnlineSessionSubsystem.h"
#include "LB_MultiplayerHubWidget.generated.h"

class SButton;
class STextBlock;
class SVerticalBox;

/** Event-driven EOS room browser. It never polls or calls the online interfaces directly. */
UCLASS(Blueprintable)
class LEFTBEHIND_API ULB_MultiplayerHubWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<ULB_OnlineSessionSubsystem> BoundOnlineSubsystem;

	TArray<FLBRoomSummary> Rooms;
	FString SelectedRoomId;

	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<STextBlock> SelectionText;
	TSharedPtr<SVerticalBox> RoomListBox;
	TSharedPtr<SButton> SignInButton;
	TSharedPtr<SButton> CreateButton;
	TSharedPtr<SButton> RefreshButton;
	TSharedPtr<SButton> JoinButton;
	TSharedPtr<SButton> BackButton;

	void BindOnlineSubsystem();
	void RefreshFromSubsystem(bool bRequestInitialSearch);
	void RefreshRoomRows();
	void UpdateControls(ELBOnlineState State);
	void ShowRequestFailure(const FText& FallbackMessage);

	UFUNCTION()
	void HandleOnlineStateChanged(ELBOnlineState NewState, const FText& StatusMessage);

	UFUNCTION()
	void HandleRoomsChanged(const TArray<FLBRoomSummary>& NewRooms);

	FReply HandleRoomSelected(FString RoomId);
	FReply HandleSignInClicked();
	FReply HandleCreateClicked();
	FReply HandleRefreshClicked();
	FReply HandleJoinClicked();
	FReply HandleBackClicked();
};
