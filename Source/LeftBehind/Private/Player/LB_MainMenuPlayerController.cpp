#include "Player/LB_MainMenuPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameMode/LB_MainMenuGameMode.h"
#include "GameMode/LB_CharacterSelectGameMode.h"
#include "GameState/LB_MainMenuGameState.h"
#include "Player/LB_PlayerState.h"
#include "System/MainMenu/LB_LocalPlayerProfileSubsystem.h"
#include "System/Online/LB_OnlineSessionSubsystem.h"
#include "UI/MainMenu/LB_MainMenuRootWidget.h"
#include "UI/MainMenu/LB_MultiplayerHubWidget.h"


DEFINE_LOG_CATEGORY_STATIC(LogLBMainMenuPlayerController, Log, All);

ALB_MainMenuPlayerController::ALB_MainMenuPlayerController()
{
	bReplicates = true;
	SetReplicateMovement(false);
	bShowMouseCursor = true;
	MainMenuWidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(
		TEXT("/Game/LeftBehind/UI/MainMenu/WBP_MainMenu.WBP_MainMenu_C")));
	MultiplayerWidgetClass = ULB_MultiplayerHubWidget::StaticClass();
	CodenameWidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(
		TEXT("/Game/LeftBehind/UI/MainMenu/WBP_CodenameEntry.WBP_CodenameEntry_C")));
	RoomNameWidgetClass = CodenameWidgetClass;
	CharacterSelectWidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(
		TEXT("/Game/LeftBehind/UI/CharacterSelect/WBP_LB_CharacterSelectWidget.WBP_LB_CharacterSelectWidget_C")));
	WaitingWidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(
		TEXT("/Game/LeftBehind/UI/MainMenu/WBP_WaitingRoom.WBP_WaitingRoom_C")));
}

void ALB_MainMenuPlayerController::SelectCharacterAndReady(ELBCharacterID CharacterID)
{
	if (!IsLocalController())
    {
        return;
    }

    if (HasAuthority())
    {
        ServerSelectCharacterAndReady_Implementation(CharacterID);
    }
    else
    {
        ServerSelectCharacterAndReady(CharacterID);
    }
}

void ALB_MainMenuPlayerController::ReadyCharacter()
{
	if (!IsLocalController())
	{
		return;
	}

	if (HasAuthority())
	{
		ServerReadyCharacter_Implementation();
		return;
	}

	ServerReadyCharacter();
}

void ALB_MainMenuPlayerController::CancelReady()
{
	if (!IsLocalController())
	{
		return;
	}

	if (HasAuthority())
	{
		ServerCancelReady_Implementation();
		return;
	}

	ServerCancelReady();
}

void ALB_MainMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_DedicatedServer || !IsLocalController())
	{
		return;
	}

	bMenuUITeardown = false;
	bShowMouseCursor = true;
	
	if (IsCharacterSelectLevel())
	{
		bMenuUITeardown = false;
		SetMenuScreen(ELBMainMenuScreen::CharacterSelect);
		return;
	}
	
	BindOnlineSubsystem();
	RefreshCodenamePlayerStateBinding();
	ShowInitialOnlineRoomScreen();
}

void ALB_MainMenuPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindCodenamePlayerState();
	UnbindOnlineSubsystem();
	TeardownMenuUI();
	Super::EndPlay(EndPlayReason);
}

void ALB_MainMenuPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	if (!IsLocalController() || bMenuUITeardown)
	{
		return;
	}

	if (IsCharacterSelectLevel())
	{
		SetMenuScreen(ELBMainMenuScreen::CharacterSelect);
		return;
	}

	RefreshCodenamePlayerStateBinding();
	ShowInitialOnlineRoomScreen();
}

void ALB_MainMenuPlayerController::PreClientTravel(
	const FString& PendingURL,
	ETravelType TravelType,
	bool bIsSeamlessTravel)
{
	UE_LOG(
		LogLBMainMenuPlayerController,
		Verbose,
		TEXT("PreClientTravel. Controller=%s Local=%d Authority=%d Seamless=%d URL=%s"),
		*GetNameSafe(this),
		IsLocalController() ? 1 : 0,
		HasAuthority() ? 1 : 0,
		bIsSeamlessTravel ? 1 : 0,
		*PendingURL);
	bCancellingCodenameFlow = true;
	CodenameEntryPurpose = ECodenameEntryPurpose::None;
	ResetCodenameSubmissionState();
	UnbindCodenamePlayerState();
	UnbindOnlineSubsystem();
	TeardownMenuUI();
	Super::PreClientTravel(PendingURL, TravelType, bIsSeamlessTravel);
}

void ALB_MainMenuPlayerController::BeginPlayingState()
{
	Super::BeginPlayingState();
	
	if (!IsLocalController())
	{
		return;
	}

	bMenuUITeardown = false;

	if (IsCharacterSelectLevel())
	{
		SetMenuScreen(ELBMainMenuScreen::CharacterSelect);
	}
}

void ALB_MainMenuPlayerController::BeginOnlinePlay()
{
	if (!IsLocalController() || bMenuUITeardown)
	{
		return;
	}
	if (bCancellingCodenameFlow || CodenameApplyState == ECodenameApplyState::Submitting)
	{
		return;
	}
	if (IsLocalNetworkPIE())
	{
		ShowInitialOnlineRoomScreen();
		return;
	}

	ULB_OnlineSessionSubsystem* OnlineSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<ULB_OnlineSessionSubsystem>()
		: nullptr;
	if (!IsValid(OnlineSubsystem))
	{
		UE_LOG(LogLBMainMenuPlayerController, Error, TEXT("EOS session subsystem is unavailable."));
		return;
	}

	if (OnlineSubsystem->IsInRoom())
	{
		ShowInitialOnlineRoomScreen();
		return;
	}

	if (ULB_LocalPlayerProfileSubsystem* Profile = GetLocalPlayerProfile())
	{
		// A new explicit GAME START always asks for a name. The cache only
		// carries the accepted value through the upcoming non-seamless travel.
		Profile->ClearCodename();
	}
	bOpenMultiplayerAfterSignIn = false;
	bCachedCodenameAutoSubmitAttempted = false;
	bCancellingCodenameFlow = false;
	CodenameEntryPurpose = ECodenameEntryPurpose::BeforeOnlinePlay;
	ResetCodenameSubmissionState();
	SetMenuScreen(ELBMainMenuScreen::Codename);
}

void ALB_MainMenuPlayerController::ContinueOnlinePlayAfterCodename()
{
	if (!IsLocalController() || bMenuUITeardown || IsLocalNetworkPIE())
	{
		return;
	}

	ULB_OnlineSessionSubsystem* OnlineSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<ULB_OnlineSessionSubsystem>()
		: nullptr;
	if (!IsValid(OnlineSubsystem))
	{
		UE_LOG(LogLBMainMenuPlayerController, Error, TEXT("EOS session subsystem is unavailable."));
		return;
	}
	if (OnlineSubsystem->IsInRoom())
	{
		ShowInitialOnlineRoomScreen();
		return;
	}

	switch (OnlineSubsystem->GetState())
	{
	case ELBOnlineState::Ready:
		bOpenMultiplayerAfterSignIn = false;
		SetMenuScreen(ELBMainMenuScreen::Multiplayer);
		break;
	case ELBOnlineState::SigningIn:
		bOpenMultiplayerAfterSignIn = true;
		SetMenuScreen(ELBMainMenuScreen::Multiplayer);
		break;
	case ELBOnlineState::SignedOut:
	case ELBOnlineState::Error:
		bOpenMultiplayerAfterSignIn = true;
		SetMenuScreen(ELBMainMenuScreen::Multiplayer);
		if (!OnlineSubsystem->SignIn() && OnlineSubsystem->GetState() == ELBOnlineState::Error)
		{
			bOpenMultiplayerAfterSignIn = false;
			SetMenuScreen(ELBMainMenuScreen::Multiplayer);
		}
		break;
	case ELBOnlineState::Searching:
	case ELBOnlineState::Creating:
	case ELBOnlineState::Joining:
		SetMenuScreen(ELBMainMenuScreen::Multiplayer);
		break;
	default:
		break;
	}
}

void ALB_MainMenuPlayerController::SetMenuScreen(ELBMainMenuScreen NewScreen)
{
	UE_LOG(LogLBMainMenuPlayerController, Warning,
		TEXT("SetMenuScreen Screen=%d Teardown=%d Local=%d"),
		(int32)NewScreen,
		bMenuUITeardown,
		IsLocalController());
	
	if (bMenuUITeardown
		|| GetNetMode() == NM_DedicatedServer
		|| !IsLocalController()
		|| NewScreen == ELBMainMenuScreen::None)
	{
		return;
	}

	if (NewScreen != ELBMainMenuScreen::Main)
	{
		bShowRoomEntryAfterMainLoad = false;
	}
	DesiredScreen = NewScreen;
	ShowDesiredMenuScreen();
}

void ALB_MainMenuPlayerController::ShowRoomEntryScreen()
{
	if (bMenuUITeardown || GetNetMode() == NM_DedicatedServer || !IsLocalController())
	{
		return;
	}

	// WBP_MainMenu's transition animations keep their final render state. The
	// instance that opened the codename screen can therefore still have faded or
	// translated children when it is added to the viewport again. Recreate the
	// widget so the designer defaults are restored before selecting Start panel.
	if (IsValid(MainMenuWidget))
	{
		MainMenuWidget->RemoveFromParent();
		MainMenuWidget = nullptr;
	}

	if (ULB_OnlineSessionSubsystem* OnlineSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<ULB_OnlineSessionSubsystem>()
		: nullptr;
		IsValid(OnlineSubsystem) && !OnlineSubsystem->IsInRoom())
	{
		if (ULB_LocalPlayerProfileSubsystem* Profile = GetLocalPlayerProfile())
		{
			Profile->ClearCodename();
		}
	}
	CodenameEntryPurpose = ECodenameEntryPurpose::None;
	bCachedCodenameAutoSubmitAttempted = false;
	bCancellingCodenameFlow = false;
	ResetCodenameSubmissionState();
	bShowRoomEntryAfterMainLoad = true;
	SetMenuScreen(ELBMainMenuScreen::Main);
}

void ALB_MainMenuPlayerController::BeginRoomCreation()
{
	if (!IsLocalController() || bMenuUITeardown || IsLocalNetworkPIE())
	{
		return;
	}

	ULB_OnlineSessionSubsystem* OnlineSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<ULB_OnlineSessionSubsystem>()
		: nullptr;
	if (!IsValid(OnlineSubsystem)
		|| OnlineSubsystem->IsInRoom()
		|| OnlineSubsystem->GetState() != ELBOnlineState::Ready)
	{
		return;
	}

	bCancellingCodenameFlow = false;
	CodenameEntryPurpose = ECodenameEntryPurpose::RoomCreation;
	ResetCodenameSubmissionState();
	SetMenuScreen(ELBMainMenuScreen::RoomName);
}

bool ALB_MainMenuPlayerController::SubmitRoomName(const FText& RawRoomName)
{
	if (!IsLocalController()
		|| bMenuUITeardown
		|| CodenameEntryPurpose != ECodenameEntryPurpose::RoomCreation)
	{
		return false;
	}

	ULB_OnlineSessionSubsystem* OnlineSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<ULB_OnlineSessionSubsystem>()
		: nullptr;
	return IsValid(OnlineSubsystem)
		&& OnlineSubsystem->CreateRoomWithName(RawRoomName.ToString());
}

bool ALB_MainMenuPlayerController::IsRoomNameEntryActive() const
{
	return CodenameEntryPurpose == ECodenameEntryPurpose::RoomCreation;
}

void ALB_MainMenuPlayerController::SubmitCodename(const FText& RawCodename)
{
	if (!IsLocalController() || bMenuUITeardown)
	{
		return;
	}
	if (bCancellingCodenameFlow || CodenameApplyState == ECodenameApplyState::Submitting)
	{
		return;
	}
	if (CodenameEntryPurpose != ECodenameEntryPurpose::BeforeOnlinePlay
		&& CodenameEntryPurpose != ECodenameEntryPurpose::InRoom)
	{
		OnCodenameSubmissionResult.Broadcast(ELBCodenameSubmitResult::NotInLobby, FString());
		return;
	}
	if (CodenameEntryPurpose == ECodenameEntryPurpose::InRoom && !HasCodenameRoomContext())
	{
		OnCodenameSubmissionResult.Broadcast(ELBCodenameSubmitResult::NotInLobby, FString());
		return;
	}

	const FString RawCodenameString = RawCodename.ToString();
	ULB_LocalPlayerProfileSubsystem* Profile = GetLocalPlayerProfile();
	if (!IsValid(Profile))
	{
		OnCodenameSubmissionResult.Broadcast(ELBCodenameSubmitResult::NotInLobby, FString());
		return;
	}

	FString SanitizedCodename;
	const ELBCodenameSubmitResult LocalResult =
		Profile->TrySetCodename(RawCodenameString, SanitizedCodename);
	if (LocalResult != ELBCodenameSubmitResult::Accepted)
	{
		// An explicit invalid edit must invalidate any value cached before
		// travel; otherwise an earlier name could be submitted behind the
		// player's back when PlayerState becomes ready.
		Profile->ClearCodename();
		bCachedCodenameAutoSubmitAttempted = true;
		OnCodenameSubmissionResult.Broadcast(LocalResult, SanitizedCodename);
		return;
	}

	if (CodenameEntryPurpose == ECodenameEntryPurpose::BeforeOnlinePlay)
	{
		CodenameEntryPurpose = ECodenameEntryPurpose::None;
		OnCodenameSubmissionResult.Broadcast(ELBCodenameSubmitResult::Accepted, SanitizedCodename);
		ContinueOnlinePlayAfterCodename();
		return;
	}

	if (!CanSubmitCodenameToCurrentRoom())
	{
		CodenameEntryPurpose = ECodenameEntryPurpose::InRoom;
		CodenameApplyState = ECodenameApplyState::WaitingForPlayerState;
		bCachedCodenameAutoSubmitAttempted = false;
		SetMenuScreen(ELBMainMenuScreen::Codename);
		return;
	}

	StartCodenameServerSubmission(SanitizedCodename, Profile->GetCodenameRevision());
}

void ALB_MainMenuPlayerController::NotifyCodenameDraftChanged(const FText& DraftCodename)
{
	if (!IsLocalController()
		|| bMenuUITeardown
		|| CodenameEntryPurpose != ECodenameEntryPurpose::InRoom
		|| CodenameApplyState == ECodenameApplyState::Submitting)
	{
		return;
	}

	ULB_LocalPlayerProfileSubsystem* Profile = GetLocalPlayerProfile();
	if (!IsValid(Profile) || !Profile->HasCodename())
	{
		return;
	}

	FString SanitizedDraft;
	const ELBCodenameSubmitResult DraftResult =
		ALB_MainMenuGameMode::ValidateCodename(DraftCodename.ToString(), SanitizedDraft);
	if (DraftResult != ELBCodenameSubmitResult::Accepted
		|| SanitizedDraft != Profile->GetCodename())
	{
		// Editing is intent. Once the draft no longer represents the cached
		// value, never auto-submit that older value after a late OnRep/state event.
		Profile->ClearCodename();
		bCachedCodenameAutoSubmitAttempted = true;
	}
}

bool ALB_MainMenuPlayerController::CancelCodenameEntry()
{
	if (!IsLocalController() || bMenuUITeardown)
	{
		return false;
	}

	if (CodenameEntryPurpose == ECodenameEntryPurpose::BeforeOnlinePlay)
	{
		ShowRoomEntryScreen();
		return true;
	}
	if (CodenameEntryPurpose == ECodenameEntryPurpose::RoomCreation)
	{
		if (ULB_OnlineSessionSubsystem* OnlineSubsystem = GetGameInstance()
			? GetGameInstance()->GetSubsystem<ULB_OnlineSessionSubsystem>()
			: nullptr;
			IsValid(OnlineSubsystem) && OnlineSubsystem->GetState() == ELBOnlineState::Creating)
		{
			return true;
		}

		CodenameEntryPurpose = ECodenameEntryPurpose::None;
		ResetCodenameSubmissionState();
		SetMenuScreen(ELBMainMenuScreen::Multiplayer);
		return true;
	}

	ULB_OnlineSessionSubsystem* OnlineSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<ULB_OnlineSessionSubsystem>()
		: nullptr;
	if (CodenameEntryPurpose != ECodenameEntryPurpose::InRoom
		|| !IsValid(OnlineSubsystem)
		|| (!IsLocalNetworkPIE() && !OnlineSubsystem->IsInRoom()))
	{
		return false;
	}
	if (IsLocalNetworkPIE())
	{
		// PIE networking has no EOS session to destroy. Consume Back instead
		// of letting Blueprint hide a still-connected room behind the main UI.
		return true;
	}

	bCancellingCodenameFlow = true;
	CodenameEntryPurpose = ECodenameEntryPurpose::None;
	ResetCodenameSubmissionState();
	if (!OnlineSubsystem->LeaveRoom())
	{
		bCancellingCodenameFlow = false;
		CodenameEntryPurpose = ECodenameEntryPurpose::InRoom;
		ReconcileInRoomCodenameFlow();
	}
	return true;
}

void ALB_MainMenuPlayerController::RequestStartCharacterSelect()
{
	if (!IsLocalListenHost())
	{
		UE_LOG(LogLBMainMenuPlayerController, Warning, TEXT("Remote start request rejected. Controller=%s"), *GetNameSafe(this));
		return;
	}

	ALB_MainMenuGameMode* MainMenuGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ALB_MainMenuGameMode>() : nullptr;
	if (!IsValid(MainMenuGameMode) || !MainMenuGameMode->TryStartCharacterSelect(this))
	{
		UE_LOG(LogLBMainMenuPlayerController, Verbose, TEXT("Start hunt request did not pass server policy."));
	}
}

bool ALB_MainMenuPlayerController::CanRequestStartCharacterSelect() const
{
	if (!IsLocalListenHost())
	{
		return false;
	}

	const ALB_MainMenuGameMode* MainMenuGameMode = GetWorld()
		? GetWorld()->GetAuthGameMode<ALB_MainMenuGameMode>()
		: nullptr;
	return IsValid(MainMenuGameMode) && MainMenuGameMode->CanStartCharacterSelect(this);
}

void ALB_MainMenuPlayerController::RequestSetLobbyReady(const bool bReady)
{
	if (!CanRequestLobbyReady())
	{
		UE_LOG(
			LogLBMainMenuPlayerController,
			Verbose,
			TEXT("Lobby ready request rejected locally. Controller=%s Ready=%d"),
			*GetNameSafe(this),
			bReady ? 1 : 0);
		return;
	}

	if (HasAuthority())
	{
		ServerSetLobbyReady_Implementation(bReady);
		return;
	}

	ServerSetLobbyReady(bReady);
}

bool ALB_MainMenuPlayerController::CanRequestLobbyReady() const
{
	if (!IsLocalController() || IsLocalListenHost() || IsCharacterSelectLevel())
	{
		return false;
	}

	const ALB_PlayerState* LBPlayerState = GetPlayerState<ALB_PlayerState>();
	const ALB_MainMenuGameState* MainMenuGameState = GetWorld()
		? GetWorld()->GetGameState<ALB_MainMenuGameState>()
		: nullptr;
	if (!IsValid(LBPlayerState)
		|| LBPlayerState->IsOnlyASpectator()
		|| !LBPlayerState->IsCodenameConfirmed()
		|| !IsValid(MainMenuGameState)
		|| MainMenuGameState->GetMainMenuSnapshot().Phase == ELBMainMenuPhase::Traveling)
	{
		return false;
	}

	// The replicated lobby actors are the authoritative indication that this
	// controller is in a ready-capable waiting room. The local EOS subsystem can
	// still be reconciling its post-travel state when the waiting UI appears, so
	// using it as an additional UI gate can leave a valid party member disabled.
	// The server validates the room, player, codename, and travel state again.
	return true;
}

bool ALB_MainMenuPlayerController::IsLocalListenHost() const
{
	const ENetMode NetMode = GetNetMode();
	return IsLocalController()
		&& HasAuthority()
		&& (NetMode == NM_ListenServer || NetMode == NM_Standalone);
}

void ALB_MainMenuPlayerController::TeardownMenuUI()
{
	if (bMenuUITeardown)
	{
		return;
	}

	bMenuUITeardown = true;
	++MenuWidgetLoadSerial;
	CancelMenuWidgetClassLoad();

	UUserWidget* Widgets[] = {
		MainMenuWidget.Get(),
		MultiplayerWidget.Get(),
		CodenameWidget.Get(),
		RoomNameWidget.Get(),
		CharacterSelectWidget.Get(),
		WaitingWidget.Get()
	};
	for (UUserWidget* Widget : Widgets)
	{
		if (IsValid(Widget))
		{
			Widget->RemoveFromParent();
		}
	}

	MainMenuWidget = nullptr;
	MultiplayerWidget = nullptr;
	CodenameWidget = nullptr;
	RoomNameWidget = nullptr;
	CharacterSelectWidget = nullptr;
	WaitingWidget = nullptr;
	DesiredScreen = ELBMainMenuScreen::None;
	VisibleScreen = ELBMainMenuScreen::None;
	bShowRoomEntryAfterMainLoad = false;
	CodenameEntryPurpose = ECodenameEntryPurpose::None;
	bCachedCodenameAutoSubmitAttempted = false;
	ResetCodenameSubmissionState();
}

void ALB_MainMenuPlayerController::ShowDesiredMenuScreen()
{
	UE_LOG(LogLBMainMenuPlayerController, Warning, TEXT("ShowDesiredMenuScreen Desired=%d"), (int32)DesiredScreen);
	
	if (bMenuUITeardown || DesiredScreen == ELBMainMenuScreen::None)
	{
		return;
	}

	if (UUserWidget* ExistingWidget = GetMenuWidget(DesiredScreen))
	{
		UE_LOG(LogLBMainMenuPlayerController, Warning,
			TEXT("Existing Widget Found: %s InViewport=%d"),
			*GetNameSafe(ExistingWidget),
			ExistingWidget->IsInViewport() ? 1 : 0);
		
		UUserWidget* Widgets[] = {
			MainMenuWidget.Get(),
			MultiplayerWidget.Get(),
			CodenameWidget.Get(),
			RoomNameWidget.Get(),
			CharacterSelectWidget.Get(),
			WaitingWidget.Get()
		};
		for (UUserWidget* Widget : Widgets)
		{
			if (IsValid(Widget) && Widget != ExistingWidget)
			{
				Widget->RemoveFromParent();
			}
		}

		if (!ExistingWidget->IsInViewport())
		{
			const bool bAdded = ExistingWidget->AddToPlayerScreen(MenuWidgetZOrder);

			UE_LOG(LogLBMainMenuPlayerController, Warning,
				TEXT("AddToPlayerScreen Result=%d Widget=%s"),
				bAdded ? 1 : 0,
				*GetNameSafe(ExistingWidget));
		}
		
		ExistingWidget->SetVisibility(ESlateVisibility::Visible);
		VisibleScreen = DesiredScreen;
		if (VisibleScreen == ELBMainMenuScreen::Main && bShowRoomEntryAfterMainLoad)
		{
			bShowRoomEntryAfterMainLoad = false;
			if (ULB_MainMenuRootWidget* MainMenuRoot = Cast<ULB_MainMenuRootWidget>(ExistingWidget))
			{
				MainMenuRoot->ShowRoomEntryPanel();
			}
			else
			{
				UE_LOG(LogLBMainMenuPlayerController, Error, TEXT("Main menu widget does not use ULB_MainMenuRootWidget."));
			}
		}
		ApplyMenuInputMode(ExistingWidget);
		if (VisibleScreen == ELBMainMenuScreen::Codename)
		{
			TrySubmitCachedCodenameIfReady();
		}
		return;
	}

	const TSoftClassPtr<UUserWidget>* WidgetClass = GetMenuWidgetClass(DesiredScreen);
	if (!WidgetClass || WidgetClass->IsNull())
	{
		UE_LOG(LogLBMainMenuPlayerController, Error, TEXT("Menu widget class is not configured. Screen=%d"), static_cast<int32>(DesiredScreen));
		return;
	}

	if (UClass* LoadedClass = WidgetClass->Get())
	{
		UE_LOG(LogLBMainMenuPlayerController, Warning, TEXT("Widget class already loaded: %s"), *LoadedClass->GetName());
		
		UUserWidget* NewWidget = CreateWidget<UUserWidget>(this, LoadedClass);
		
		UE_LOG(LogLBMainMenuPlayerController, Warning, TEXT("CreateWidget Result: %s"), *GetNameSafe(NewWidget));
		
		if (IsValid(NewWidget))
		{
			SetMenuWidget(DesiredScreen, NewWidget);
			ShowDesiredMenuScreen();
		}
		return;
	}

	CancelMenuWidgetClassLoad();
	const ELBMainMenuScreen RequestedScreen = DesiredScreen;
	const uint32 LoadSerial = ++MenuWidgetLoadSerial;
	MenuWidgetLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		WidgetClass->ToSoftObjectPath(),
		FStreamableDelegate::CreateUObject(
			this,
			&ThisClass::HandleMenuWidgetClassLoaded,
			RequestedScreen,
			LoadSerial),
		FStreamableManager::DefaultAsyncLoadPriority,
		false,
		false,
		TEXT("LB_MainMenuWidget"));
}

void ALB_MainMenuPlayerController::HandleMenuWidgetClassLoaded(
	ELBMainMenuScreen LoadedScreen,
	uint32 LoadSerial)
{
	MenuWidgetLoadHandle.Reset();
	if (bMenuUITeardown || LoadSerial != MenuWidgetLoadSerial || LoadedScreen != DesiredScreen)
	{
		return;
	}

	const TSoftClassPtr<UUserWidget>* WidgetClass = GetMenuWidgetClass(LoadedScreen);
	UClass* LoadedClass = WidgetClass ? WidgetClass->Get() : nullptr;
	if (!IsValid(LoadedClass))
	{
		UE_LOG(LogLBMainMenuPlayerController, Error, TEXT("Async menu widget load completed without a class. Screen=%d"), static_cast<int32>(LoadedScreen));
		return;
	}

	UUserWidget* NewWidget = CreateWidget<UUserWidget>(this, LoadedClass);
	
	UE_LOG(LogLBMainMenuPlayerController, Warning, TEXT("Async CreateWidget Result: %s"), *GetNameSafe(NewWidget));
	
	if (!IsValid(NewWidget))
	{
		UE_LOG(LogLBMainMenuPlayerController, Error, TEXT("Failed to create menu widget. Screen=%d"), static_cast<int32>(LoadedScreen));
		return;
	}

	UE_LOG(LogLBMainMenuPlayerController, Warning, TEXT("Set Character Widget Screen=%d Widget=%s"), (int32)LoadedScreen, *GetNameSafe(NewWidget));
	
	SetMenuWidget(LoadedScreen, NewWidget);
	ShowDesiredMenuScreen();
}

void ALB_MainMenuPlayerController::CancelMenuWidgetClassLoad()
{
	if (!MenuWidgetLoadHandle.IsValid())
	{
		return;
	}

	if (!MenuWidgetLoadHandle->HasLoadCompleted())
	{
		MenuWidgetLoadHandle->CancelHandle();
	}
	MenuWidgetLoadHandle.Reset();
}

UUserWidget* ALB_MainMenuPlayerController::GetMenuWidget(ELBMainMenuScreen Screen) const
{
	switch (Screen)
	{
	case ELBMainMenuScreen::Main:
		return MainMenuWidget;
	case ELBMainMenuScreen::Multiplayer:
		return MultiplayerWidget;
	case ELBMainMenuScreen::Codename:
		return CodenameWidget;
	case ELBMainMenuScreen::RoomName:
		return RoomNameWidget;
	case ELBMainMenuScreen::CharacterSelect:
		return CharacterSelectWidget;
	case ELBMainMenuScreen::Waiting:
		return WaitingWidget;
	default:
		return nullptr;
	}
}

void ALB_MainMenuPlayerController::SetMenuWidget(ELBMainMenuScreen Screen, UUserWidget* Widget)
{
	switch (Screen)
	{
	case ELBMainMenuScreen::Main:
		MainMenuWidget = Widget;
		break;
	case ELBMainMenuScreen::Multiplayer:
		MultiplayerWidget = Widget;
		break;
	case ELBMainMenuScreen::Codename:
		CodenameWidget = Widget;
		break;
	case ELBMainMenuScreen::RoomName:
		RoomNameWidget = Widget;
		break;
	case ELBMainMenuScreen::CharacterSelect:
		CharacterSelectWidget = Widget;
		break;
	case ELBMainMenuScreen::Waiting:
		WaitingWidget = Widget;
		break;
	default:
		break;
	}
}

const TSoftClassPtr<UUserWidget>* ALB_MainMenuPlayerController::GetMenuWidgetClass(ELBMainMenuScreen Screen) const
{
	switch (Screen)
	{
	case ELBMainMenuScreen::Main:
		return &MainMenuWidgetClass;
	case ELBMainMenuScreen::Multiplayer:
		return &MultiplayerWidgetClass;
	case ELBMainMenuScreen::Codename:
		return &CodenameWidgetClass;
	case ELBMainMenuScreen::RoomName:
		return &RoomNameWidgetClass;
	case ELBMainMenuScreen::CharacterSelect:
		return &CharacterSelectWidgetClass;
	case ELBMainMenuScreen::Waiting:
		return &WaitingWidgetClass;
	default:
		return nullptr;
	}
}

void ALB_MainMenuPlayerController::ApplyMenuInputMode(UUserWidget* FocusWidget)
{
	(void)FocusWidget;
	bShowMouseCursor = true;
	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}

ULB_LocalPlayerProfileSubsystem* ALB_MainMenuPlayerController::GetLocalPlayerProfile() const
{
	return GetGameInstance()
		? GetGameInstance()->GetSubsystem<ULB_LocalPlayerProfileSubsystem>()
		: nullptr;
}

bool ALB_MainMenuPlayerController::HasCodenameRoomContext() const
{
	if (IsLocalNetworkPIE())
	{
		return true;
	}

	const ULB_OnlineSessionSubsystem* OnlineSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<ULB_OnlineSessionSubsystem>()
		: nullptr;
	return IsValid(OnlineSubsystem) && OnlineSubsystem->IsInRoom();
}

bool ALB_MainMenuPlayerController::CanSubmitCodenameToCurrentRoom() const
{
	if (!HasCodenameRoomContext())
	{
		return false;
	}

	if (!IsLocalNetworkPIE())
	{
		const ULB_OnlineSessionSubsystem* OnlineSubsystem = GetGameInstance()
			? GetGameInstance()->GetSubsystem<ULB_OnlineSessionSubsystem>()
			: nullptr;
		if (!IsValid(OnlineSubsystem) || OnlineSubsystem->GetState() != ELBOnlineState::InRoom)
		{
			return false;
		}
	}

	const ALB_PlayerState* LBPlayerState = GetPlayerState<ALB_PlayerState>();
	return IsValid(LBPlayerState)
		&& LBPlayerState->GetWorld() == GetWorld()
		&& !LBPlayerState->IsOnlyASpectator();
}

void ALB_MainMenuPlayerController::ResetCodenameSubmissionState()
{
	CodenameApplyState = ECodenameApplyState::Idle;
	ActiveCodenameRequestId = 0;
	ActiveCodenameRevision = 0;
	ActiveSubmittedCodename.Reset();
	ActiveCodenameTarget.Reset();
}

void ALB_MainMenuPlayerController::RefreshCodenamePlayerStateBinding()
{
	ALB_PlayerState* CurrentPlayerState = GetPlayerState<ALB_PlayerState>();
	if (BoundCodenamePlayerState.Get() == CurrentPlayerState)
	{
		return;
	}

	UnbindCodenamePlayerState();
	if (IsValid(CurrentPlayerState))
	{
		CurrentPlayerState->OnCodenameConfirmedChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleCodenameConfirmedChanged);
		BoundCodenamePlayerState = CurrentPlayerState;
	}
}

void ALB_MainMenuPlayerController::UnbindCodenamePlayerState()
{
	if (ALB_PlayerState* PreviousPlayerState = BoundCodenamePlayerState.Get())
	{
		PreviousPlayerState->OnCodenameConfirmedChanged.RemoveDynamic(
			this,
			&ThisClass::HandleCodenameConfirmedChanged);
	}
	BoundCodenamePlayerState.Reset();
}

void ALB_MainMenuPlayerController::ReconcileInRoomCodenameFlow()
{
	if (!IsLocalController() || bMenuUITeardown || bCancellingCodenameFlow)
	{
		return;
	}

	const ALB_PlayerState* LBPlayerState = GetPlayerState<ALB_PlayerState>();
	if (IsValid(LBPlayerState) && LBPlayerState->IsCodenameConfirmed())
	{
		CodenameEntryPurpose = ECodenameEntryPurpose::None;
		bCachedCodenameAutoSubmitAttempted = false;
		// On a listen host the replicated delegate fires synchronously inside
		// the server confirmation call. Preserve the active request until its
		// matching client result arrives so the Accepted delegate still fires.
		if (CodenameApplyState != ECodenameApplyState::Submitting)
		{
			ResetCodenameSubmissionState();
		}
		SetMenuScreen(ELBMainMenuScreen::Waiting);
		return;
	}

	CodenameEntryPurpose = ECodenameEntryPurpose::InRoom;
	if (CodenameApplyState != ECodenameApplyState::Submitting)
	{
		CodenameApplyState = IsValid(LBPlayerState)
			? ECodenameApplyState::Idle
			: ECodenameApplyState::WaitingForPlayerState;
	}
	// The widget is shown before auto-submit so a server rejection always has
	// a live result listener and the cached value can be edited and retried.
	SetMenuScreen(ELBMainMenuScreen::Codename);
}

void ALB_MainMenuPlayerController::TrySubmitCachedCodenameIfReady()
{
	if (CodenameEntryPurpose != ECodenameEntryPurpose::InRoom
		|| bCancellingCodenameFlow
		|| bCachedCodenameAutoSubmitAttempted
		|| CodenameApplyState == ECodenameApplyState::Submitting)
	{
		return;
	}

	ULB_LocalPlayerProfileSubsystem* Profile = GetLocalPlayerProfile();
	if (!IsValid(Profile) || !Profile->HasCodename())
	{
		return;
	}
	if (!CanSubmitCodenameToCurrentRoom())
	{
		CodenameApplyState = ECodenameApplyState::WaitingForPlayerState;
		return;
	}

	bCachedCodenameAutoSubmitAttempted = true;
	StartCodenameServerSubmission(Profile->GetCodename(), Profile->GetCodenameRevision());
}

void ALB_MainMenuPlayerController::StartCodenameServerSubmission(
	const FString& SanitizedCodename,
	const uint32 CodenameRevision)
{
	if (CodenameApplyState == ECodenameApplyState::Submitting
		|| bCancellingCodenameFlow
		|| !CanSubmitCodenameToCurrentRoom())
	{
		return;
	}

	ULB_LocalPlayerProfileSubsystem* Profile = GetLocalPlayerProfile();
	ALB_PlayerState* LBPlayerState = GetPlayerState<ALB_PlayerState>();
	if (!IsValid(Profile)
		|| !IsValid(LBPlayerState)
		|| !Profile->HasCodename()
		|| Profile->GetCodenameRevision() != CodenameRevision
		|| Profile->GetCodename() != SanitizedCodename)
	{
		return;
	}

	++NextCodenameRequestId;
	if (NextCodenameRequestId == 0)
	{
		++NextCodenameRequestId;
	}
	ActiveCodenameRequestId = NextCodenameRequestId;
	ActiveCodenameRevision = CodenameRevision;
	ActiveSubmittedCodename = SanitizedCodename;
	ActiveCodenameTarget = LBPlayerState;
	CodenameApplyState = ECodenameApplyState::Submitting;
	CodenameEntryPurpose = ECodenameEntryPurpose::InRoom;

	if (HasAuthority())
	{
		HandleCodenameSubmission_ServerOnly(SanitizedCodename, ActiveCodenameRequestId);
		return;
	}

	ServerSubmitCodename(SanitizedCodename, ActiveCodenameRequestId);
}

void ALB_MainMenuPlayerController::BindOnlineSubsystem()
{
	if (ULB_OnlineSessionSubsystem* OnlineSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<ULB_OnlineSessionSubsystem>()
		: nullptr)
	{
		OnlineSubsystem->OnStateChanged.AddUniqueDynamic(this, &ThisClass::HandleOnlineStateChanged);
	}
}

void ALB_MainMenuPlayerController::UnbindOnlineSubsystem()
{
	if (ULB_OnlineSessionSubsystem* OnlineSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<ULB_OnlineSessionSubsystem>()
		: nullptr)
	{
		OnlineSubsystem->OnStateChanged.RemoveDynamic(this, &ThisClass::HandleOnlineStateChanged);
	}
}

void ALB_MainMenuPlayerController::ShowInitialOnlineRoomScreen()
{
	if (IsLocalNetworkPIE())
	{
		ReconcileInRoomCodenameFlow();
		return;
	}

	ULB_OnlineSessionSubsystem* OnlineSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<ULB_OnlineSessionSubsystem>()
		: nullptr;
	if (!IsValid(OnlineSubsystem))
	{
		SetMenuScreen(ELBMainMenuScreen::Main);
		return;
	}
	if (!OnlineSubsystem->IsInRoom())
	{
		CodenameEntryPurpose = ECodenameEntryPurpose::None;
		bCachedCodenameAutoSubmitAttempted = false;
		ResetCodenameSubmissionState();
		SetMenuScreen(OnlineSubsystem->GetState() == ELBOnlineState::Error
			? ELBMainMenuScreen::Multiplayer
			: ELBMainMenuScreen::Main);
		return;
	}

	ReconcileInRoomCodenameFlow();
}

bool ALB_MainMenuPlayerController::IsLocalNetworkPIE() const
{
#if WITH_EDITOR
	const UWorld* World = GetWorld();
	return IsValid(World)
		&& World->WorldType == EWorldType::PIE
		&& GetNetMode() != NM_Standalone;
#else
	return false;
#endif
}

void ALB_MainMenuPlayerController::HandleOnlineStateChanged(
	ELBOnlineState NewState,
	const FText& StatusMessage)
{
	(void)StatusMessage;
	if (NewState == ELBOnlineState::InRoom)
	{
		bOpenMultiplayerAfterSignIn = false;
		bCancellingCodenameFlow = false;
		ShowInitialOnlineRoomScreen();
		return;
	}
	if (NewState == ELBOnlineState::Leaving)
	{
		bCancellingCodenameFlow = true;
		CodenameEntryPurpose = ECodenameEntryPurpose::None;
		ResetCodenameSubmissionState();
		return;
	}

	if (bOpenMultiplayerAfterSignIn && NewState == ELBOnlineState::Ready)
	{
		bOpenMultiplayerAfterSignIn = false;
		SetMenuScreen(ELBMainMenuScreen::Multiplayer);
	}
	else if (bOpenMultiplayerAfterSignIn && NewState == ELBOnlineState::Error)
	{
		bOpenMultiplayerAfterSignIn = false;
		SetMenuScreen(ELBMainMenuScreen::Multiplayer);
	}
}

void ALB_MainMenuPlayerController::HandleCodenameConfirmedChanged(const bool bConfirmed)
{
	if (!bConfirmed
		|| !IsLocalController()
		|| bMenuUITeardown
		|| bCancellingCodenameFlow
		|| IsCharacterSelectLevel())
	{
		return;
	}

	ReconcileInRoomCodenameFlow();
}

void ALB_MainMenuPlayerController::ServerSelectCharacterAndReady_Implementation(ELBCharacterID CharacterID)
{
	ALB_CharacterSelectGameMode* GameMode =
		GetWorld()
		? GetWorld()->GetAuthGameMode<ALB_CharacterSelectGameMode>()
		: nullptr;

	if (!IsValid(GameMode))
	{
		return;
	}

	const ELBCharacterSelectResult Result =
		GameMode->TrySelectCharacter(this, CharacterID);

	ClientReceiveCharacterSelectResult(Result);

	if (Result == ELBCharacterSelectResult::Success)
	{
		GameMode->TrySetCharacterReady(this);
	}
}

void ALB_MainMenuPlayerController::Server_SelectCharacterPreview_Implementation(ELBCharacterID CharacterID)
{
	if (ALB_PlayerState* PS = GetPlayerState<ALB_PlayerState>())
	{
		PS->SetCharacterID_ServerOnly(CharacterID);
	}

	if (ALB_CharacterSelectGameMode* GM =
		GetWorld()->GetAuthGameMode<ALB_CharacterSelectGameMode>())
	{
		GM->RefreshSnapshot();
	}
}

bool ALB_MainMenuPlayerController::IsCharacterSelectLevel() const
{
	const UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return false;
	}

	return World->GetMapName().Contains(TEXT("L_CharacterSelect"));
}

void ALB_MainMenuPlayerController::ClientReceiveCharacterSelectResult_Implementation(ELBCharacterSelectResult Result)
{
	OnCharacterSelectResult.Broadcast(Result);
}

void ALB_MainMenuPlayerController::ServerReadyCharacter_Implementation()
{
	ALB_CharacterSelectGameMode* GameMode =
		GetWorld() ? GetWorld()->GetAuthGameMode<ALB_CharacterSelectGameMode>() : nullptr;

	if (!IsValid(GameMode))
	{
		return;
	}

	GameMode->TrySetCharacterReady(this);
}

void ALB_MainMenuPlayerController::ServerCancelReady_Implementation()
{
	ALB_CharacterSelectGameMode* GameMode =
		GetWorld() ? GetWorld()->GetAuthGameMode<ALB_CharacterSelectGameMode>() : nullptr;

	if (!IsValid(GameMode))
	{
		return;
	}

	GameMode->TryCancelCharacterReady(this);
}


void ALB_MainMenuPlayerController::HandleCodenameSubmission_ServerOnly(
	const FString& RawCodename,
	const uint32 RequestId)
{
	if (!HasAuthority())
	{
		return;
	}

	FString SanitizedCodename;
	ALB_MainMenuGameMode* MainMenuGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ALB_MainMenuGameMode>() : nullptr;
	const ELBCodenameSubmitResult Result = IsValid(MainMenuGameMode)
		? MainMenuGameMode->TryConfirmCodename(this, RawCodename, SanitizedCodename)
		: ELBCodenameSubmitResult::NotInLobby;
	ClientReceiveCodenameSubmissionResult(Result, SanitizedCodename, RequestId);
}

void ALB_MainMenuPlayerController::ServerSubmitCodename_Implementation(
	const FString& RawCodename,
	const uint32 RequestId)
{
	HandleCodenameSubmission_ServerOnly(RawCodename, RequestId);
}

void ALB_MainMenuPlayerController::ServerSetLobbyReady_Implementation(const bool bReady)
{
	ALB_MainMenuGameMode* MainMenuGameMode = GetWorld()
		? GetWorld()->GetAuthGameMode<ALB_MainMenuGameMode>()
		: nullptr;
	if (!IsValid(MainMenuGameMode) || !MainMenuGameMode->TrySetLobbyReady(this, bReady))
	{
		UE_LOG(
			LogLBMainMenuPlayerController,
			Verbose,
			TEXT("Lobby ready request did not pass server policy. Controller=%s Ready=%d"),
			*GetNameSafe(this),
			bReady ? 1 : 0);
	}
}

void ALB_MainMenuPlayerController::ClientReceiveCodenameSubmissionResult_Implementation(
	ELBCodenameSubmitResult Result,
	const FString& SanitizedCodename,
	const uint32 RequestId)
{
	if (RequestId == 0
		|| RequestId != ActiveCodenameRequestId
		|| CodenameApplyState != ECodenameApplyState::Submitting)
	{
		UE_LOG(
			LogLBMainMenuPlayerController,
			Verbose,
			TEXT("Ignoring stale codename result. Request=%u Active=%u Result=%d"),
			RequestId,
			ActiveCodenameRequestId,
			static_cast<int32>(Result));
		return;
	}

	ULB_LocalPlayerProfileSubsystem* Profile = GetLocalPlayerProfile();
	const bool bCurrentRequest = !bCancellingCodenameFlow
		&& IsValid(Profile)
		&& Profile->GetCodenameRevision() == ActiveCodenameRevision
		&& Profile->GetCodename() == ActiveSubmittedCodename
		&& ActiveCodenameTarget.Get() == GetPlayerState<ALB_PlayerState>();
	ResetCodenameSubmissionState();
	if (!bCurrentRequest)
	{
		return;
	}

	if (Result == ELBCodenameSubmitResult::Accepted)
	{
		CodenameEntryPurpose = ECodenameEntryPurpose::None;
		SetMenuScreen(ELBMainMenuScreen::Waiting);
		OnCodenameSubmissionResult.Broadcast(Result, SanitizedCodename);
		return;
	}

	if (Result == ELBCodenameSubmitResult::TooShort
		|| Result == ELBCodenameSubmitResult::TooLong
		|| Result == ELBCodenameSubmitResult::InvalidCharacters)
	{
		Profile->ClearCodename();
	}
	CodenameEntryPurpose = ECodenameEntryPurpose::InRoom;
	SetMenuScreen(ELBMainMenuScreen::Codename);
	OnCodenameSubmissionResult.Broadcast(Result, SanitizedCodename);
}
