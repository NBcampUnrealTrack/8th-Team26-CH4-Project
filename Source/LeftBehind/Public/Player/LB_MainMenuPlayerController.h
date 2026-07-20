#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "System/MainMenu/LBMainMenuTypes.h"
#include "System/Online/LB_OnlineSessionSubsystem.h"
#include "System/Character/LBCharacterTypes.h"
#include "LB_MainMenuPlayerController.generated.h"

enum class ELBCharacterSelectResult : uint8;
struct FStreamableHandle;
class ALB_PlayerState;
class ULB_LocalPlayerProfileSubsystem;
class UUserWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLBCodenameSubmissionResult,
	ELBCodenameSubmitResult,
	Result,
	const FString&,
	SanitizedCodename);

// 멀티 환경 캐릭터 셀렉 현황 -- UI에서 구독
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLBOnCharacterSelectResult, ELBCharacterSelectResult, Result);

UCLASS()
class LEFTBEHIND_API ALB_MainMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ALB_MainMenuPlayerController();

	UFUNCTION(BlueprintCallable, Category="LB|MainMenu|UI")
	void SetMenuScreen(ELBMainMenuScreen NewScreen);

	/** Returns from the EOS room browser to the existing Start/room-entry panel. */
	UFUNCTION(BlueprintCallable, Category="LB|MainMenu|UI")
	void ShowRoomEntryScreen();

	/** Starts EOS sign-in and opens the room browser once the account is ready. */
	UFUNCTION(BlueprintCallable, Category="LB|MainMenu|Online")
	void BeginOnlinePlay();

	/** Opens the standalone room-creation screen. */
	UFUNCTION(BlueprintCallable, Category="LB|MainMenu|Online")
	void BeginRoomCreation();

	/** Validates the current room-name draft and starts EOS room creation. */
	UFUNCTION(BlueprintCallable, Category="LB|MainMenu|Online")
	bool SubmitRoomName(const FText& RawRoomName);

	/** Returns from room creation to the room browser unless creation is already in flight. */
	UFUNCTION(BlueprintCallable, Category="LB|MainMenu|Online")
	bool CancelRoomCreation();

	UFUNCTION(BlueprintCallable, Category="LB|MainMenu|Name")
	void SubmitCodename(const FText& RawCodename);

	/** Invalidates a travel-cached value as soon as the visible draft diverges from it. */
	void NotifyCodenameDraftChanged(const FText& DraftCodename);

	/** Handles Back from the codename screen without leaving a hidden online room behind. */
	bool CancelCodenameEntry();

	// Listen Host의 로컬 authority 인스턴스에만 실행 경로가 존재한다. 원격 travel RPC는 의도적으로 없다.
	UFUNCTION(BlueprintCallable, Category="LB|MainMenu|Travel")
	void RequestStartCharacterSelect();

	UFUNCTION(BlueprintPure, Category="LB|MainMenu|Travel")
	bool CanRequestStartCharacterSelect() const;

	/** Sets this non-host client's explicit ready state in the waiting room. */
	UFUNCTION(BlueprintCallable, Category="LB|MainMenu|Ready")
	void RequestSetLobbyReady(bool bReady);

	UFUNCTION(BlueprintPure, Category="LB|MainMenu|Ready")
	bool CanRequestLobbyReady() const;

	UFUNCTION(BlueprintPure, Category="LB|MainMenu|Network")
	bool IsLocalListenHost() const;

	UFUNCTION(BlueprintCallable, Category="LB|MainMenu|UI")
	void TeardownMenuUI();

	UPROPERTY(BlueprintAssignable, Category="LB|MainMenu|Name")
	FOnLBCodenameSubmissionResult OnCodenameSubmissionResult;
	
	UPROPERTY(BlueprintAssignable)
	FLBOnCharacterSelectResult OnCharacterSelectResult;
	
	UFUNCTION(BlueprintCallable, Category="LB|MainMenu|Character")
	void SelectCharacterAndReady(ELBCharacterID CharacterID);
	
	UFUNCTION(BlueprintCallable, Category="LB|MainMenu|Character")
	void ReadyCharacter();
	
	UFUNCTION(BlueprintCallable, Category="LB|MainMenu|Character")
	void CancelReady();

	UFUNCTION(Server, Reliable)
	void Server_SelectCharacterPreview(ELBCharacterID CharacterID);
	
	
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnRep_PlayerState() override;
	virtual void PreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel) override;
	virtual void NotifyLoadedWorld(FName WorldPackageName, bool bFinalDest) override;

	virtual void BeginPlayingState() override;
	
	UPROPERTY(EditDefaultsOnly, Category="LB|MainMenu|UI")
	TSoftClassPtr<UUserWidget> MainMenuWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category="LB|MainMenu|UI")
	TSoftClassPtr<UUserWidget> CodenameWidgetClass;

	/** Independent native screen used only for room creation. */
	UPROPERTY(EditDefaultsOnly, Category="LB|MainMenu|UI")
	TSoftClassPtr<UUserWidget> RoomNameWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category="LB|MainMenu|UI")
	TSoftClassPtr<UUserWidget> MultiplayerWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category="LB|MainMenu|UI")
	TSoftClassPtr<UUserWidget> CharacterSelectWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category="LB|MainMenu|UI")
	TSoftClassPtr<UUserWidget> WaitingWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category="LB|MainMenu|UI")
	int32 MenuWidgetZOrder = 20;

private:
	enum class ECodenameEntryPurpose : uint8
	{
		None,
		BeforeOnlinePlay,
		InRoom
	};

	enum class ECodenameApplyState : uint8
	{
		Idle,
		WaitingForPlayerState,
		Submitting
	};

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> MainMenuWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> CodenameWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> RoomNameWidget;

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
	bool bShowRoomEntryAfterMainLoad = false;
	bool bCachedCodenameAutoSubmitAttempted = false;
	bool bCancellingCodenameFlow = false;
	bool bRoomCreationActive = false;
	FTimerHandle RaidDestinationLoadedRetryTimerHandle;
	ECodenameEntryPurpose CodenameEntryPurpose = ECodenameEntryPurpose::None;
	ECodenameApplyState CodenameApplyState = ECodenameApplyState::Idle;
	uint32 NextCodenameRequestId = 0;
	uint32 ActiveCodenameRequestId = 0;
	uint32 ActiveCodenameRevision = 0;
	FString ActiveSubmittedCodename;
	TWeakObjectPtr<ALB_PlayerState> ActiveCodenameTarget;
	TWeakObjectPtr<ALB_PlayerState> BoundCodenamePlayerState;

	void ShowDesiredMenuScreen();
	void HandleMenuWidgetClassLoaded(ELBMainMenuScreen LoadedScreen, uint32 LoadSerial);
	void CancelMenuWidgetClassLoad();
	UUserWidget* GetMenuWidget(ELBMainMenuScreen Screen) const;
	void SetMenuWidget(ELBMainMenuScreen Screen, UUserWidget* Widget);
	const TSoftClassPtr<UUserWidget>* GetMenuWidgetClass(ELBMainMenuScreen Screen) const;
	void ApplyMenuInputMode(UUserWidget* FocusWidget);
	void ContinueOnlinePlayAfterCodename();
	void ReconcileInRoomCodenameFlow();
	void TrySubmitCachedCodenameIfReady();
	void StartCodenameServerSubmission(const FString& SanitizedCodename, uint32 CodenameRevision);
	void ResetCodenameSubmissionState();
	bool HasCodenameRoomContext() const;
	bool CanSubmitCodenameToCurrentRoom() const;
	ULB_LocalPlayerProfileSubsystem* GetLocalPlayerProfile() const;
	void RefreshCodenamePlayerStateBinding();
	void UnbindCodenamePlayerState();
	void HandleCodenameSubmission_ServerOnly(const FString& RawCodename, uint32 RequestId);
	void BindOnlineSubsystem();
	void UnbindOnlineSubsystem();
	void ShowInitialOnlineRoomScreen();
	bool IsLocalNetworkPIE() const;
	void SendRaidDestinationLoadedNotification();

	UFUNCTION()
	void HandleOnlineStateChanged(ELBOnlineState NewState, const FText& StatusMessage);

	UFUNCTION()
	void HandleCodenameConfirmedChanged(bool bConfirmed);

	UFUNCTION(Server, Reliable)
	void ServerSubmitCodename(const FString& RawCodename, uint32 RequestId);

	UFUNCTION(Server, Reliable)
	void ServerSetLobbyReady(bool bReady);

	UFUNCTION(Server, Reliable)
	void ServerNotifyRaidDestinationLoaded();

	UFUNCTION(Client, Reliable)
	void ClientReceiveCodenameSubmissionResult(
		ELBCodenameSubmitResult Result,
		const FString& SanitizedCodename,
		uint32 RequestId);
	
	UFUNCTION(Server, Reliable)
	void ServerReadyCharacter();

	UFUNCTION(Server, Reliable)
	void ServerCancelReady();
	
	UFUNCTION(Client, Reliable)
	void ClientReceiveCharacterSelectResult(ELBCharacterSelectResult Result);
	
	UFUNCTION(Server, Reliable)
	void ServerSelectCharacterAndReady(ELBCharacterID CharacterID);
	
	// CharacterSelect 레벨인지 판단
	bool IsCharacterSelectLevel() const;
};
