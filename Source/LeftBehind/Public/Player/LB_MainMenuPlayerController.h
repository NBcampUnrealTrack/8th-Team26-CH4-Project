#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "System/MainMenu/LBMainMenuTypes.h"
#include "System/Online/LB_OnlineSessionSubsystem.h"
#include "LB_MainMenuPlayerController.generated.h"

struct FStreamableHandle;
class UUserWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnLBCodenameSubmissionResult,
	ELBCodenameSubmitResult,
	Result,
	const FString&,
	SanitizedCodename);

UCLASS()
class LEFTBEHIND_API ALB_MainMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ALB_MainMenuPlayerController();

	UFUNCTION(BlueprintCallable, Category="LB|MainMenu|UI")
	void SetMenuScreen(ELBMainMenuScreen NewScreen);

	/** Starts EOS sign-in and opens the room browser once the account is ready. */
	UFUNCTION(BlueprintCallable, Category="LB|MainMenu|Online")
	void BeginOnlinePlay();

	UFUNCTION(BlueprintCallable, Category="LB|MainMenu|Name")
	void SubmitCodename(const FText& RawCodename);

	// Listen Host의 로컬 authority 인스턴스에만 실행 경로가 존재한다. 원격 travel RPC는 의도적으로 없다.
	UFUNCTION(BlueprintCallable, Category="LB|MainMenu|Travel")
	void RequestStartHunt();

	UFUNCTION(BlueprintPure, Category="LB|MainMenu|Travel")
	bool CanRequestStartHunt() const;

	UFUNCTION(BlueprintPure, Category="LB|MainMenu|Network")
	bool IsLocalListenHost() const;

	UFUNCTION(BlueprintCallable, Category="LB|MainMenu|UI")
	void TeardownMenuUI();

	UPROPERTY(BlueprintAssignable, Category="LB|MainMenu|Name")
	FOnLBCodenameSubmissionResult OnCodenameSubmissionResult;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnRep_PlayerState() override;
	virtual void PreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel) override;

	UPROPERTY(EditDefaultsOnly, Category="LB|MainMenu|UI")
	TSoftClassPtr<UUserWidget> MainMenuWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category="LB|MainMenu|UI")
	TSoftClassPtr<UUserWidget> CodenameWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category="LB|MainMenu|UI")
	TSoftClassPtr<UUserWidget> MultiplayerWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category="LB|MainMenu|UI")
	TSoftClassPtr<UUserWidget> CharacterSelectWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category="LB|MainMenu|UI")
	TSoftClassPtr<UUserWidget> WaitingWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category="LB|MainMenu|UI")
	int32 MenuWidgetZOrder = 20;

private:
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> MainMenuWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> CodenameWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> MultiplayerWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> CharacterSelectWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> WaitingWidget;

	ELBMainMenuScreen DesiredScreen = ELBMainMenuScreen::None;
	ELBMainMenuScreen VisibleScreen = ELBMainMenuScreen::None;
	TSharedPtr<FStreamableHandle> MenuWidgetLoadHandle;
	uint32 MenuWidgetLoadSerial = 0;
	bool bMenuUITeardown = false;
	bool bOpenMultiplayerAfterSignIn = false;

	void ShowDesiredMenuScreen();
	void HandleMenuWidgetClassLoaded(ELBMainMenuScreen LoadedScreen, uint32 LoadSerial);
	void CancelMenuWidgetClassLoad();
	UUserWidget* GetMenuWidget(ELBMainMenuScreen Screen) const;
	void SetMenuWidget(ELBMainMenuScreen Screen, UUserWidget* Widget);
	const TSoftClassPtr<UUserWidget>* GetMenuWidgetClass(ELBMainMenuScreen Screen) const;
	void ApplyMenuInputMode(UUserWidget* FocusWidget);
	void HandleCodenameSubmission_ServerOnly(const FString& RawCodename);
	void BindOnlineSubsystem();
	void UnbindOnlineSubsystem();
	void ShowInitialOnlineRoomScreen();
	bool IsLocalNetworkPIE() const;

	UFUNCTION()
	void HandleOnlineStateChanged(ELBOnlineState NewState, const FText& StatusMessage);

	UFUNCTION(Server, Reliable)
	void ServerSubmitCodename(const FString& RawCodename);

	UFUNCTION(Client, Reliable)
	void ClientReceiveCodenameSubmissionResult(
		ELBCodenameSubmitResult Result,
		const FString& SanitizedCodename);
};
