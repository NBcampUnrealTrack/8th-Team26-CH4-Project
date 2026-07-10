#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "System/MainMenu/LBMainMenuTypes.h"
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

	TSharedPtr<STextBlock> PlayerCountText;
	TSharedPtr<STextBlock> ConfirmedCountText;
	TSharedPtr<STextBlock> TargetMapText;
	TSharedPtr<STextBlock> PhaseText;
	TSharedPtr<SVerticalBox> PlayerListBox;
	TSharedPtr<SButton> StartButton;

	UFUNCTION()
	void HandleSnapshotChanged(const FLBMainMenuSnapshot& Snapshot);

	void BindGameState();
	void Refresh(const FLBMainMenuSnapshot& Snapshot);
	FReply HandleStartClicked();
};
