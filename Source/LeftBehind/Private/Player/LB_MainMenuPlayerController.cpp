#include "Player/LB_MainMenuPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameMode/LB_MainMenuGameMode.h"

DEFINE_LOG_CATEGORY_STATIC(LogLBMainMenuPlayerController, Log, All);

ALB_MainMenuPlayerController::ALB_MainMenuPlayerController()
{
	bReplicates = true;
	SetReplicateMovement(false);
	bShowMouseCursor = true;
	MainMenuWidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(
		TEXT("/Game/LeftBehind/UI/MainMenu/WBP_MainMenu.WBP_MainMenu_C")));
	CodenameWidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(
		TEXT("/Game/LeftBehind/UI/MainMenu/WBP_CodenameEntry.WBP_CodenameEntry_C")));
	CharacterSelectWidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(
		TEXT("/Game/LeftBehind/UI/MainMenu/WBP_CharacterSelect.WBP_CharacterSelect_C")));
	WaitingWidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(
		TEXT("/Game/LeftBehind/UI/MainMenu/WBP_WaitingRoom.WBP_WaitingRoom_C")));
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
	SetMenuScreen(ELBMainMenuScreen::Main);
}

void ALB_MainMenuPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TeardownMenuUI();
	Super::EndPlay(EndPlayReason);
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
	TeardownMenuUI();
	Super::PreClientTravel(PendingURL, TravelType, bIsSeamlessTravel);
}

void ALB_MainMenuPlayerController::SetMenuScreen(ELBMainMenuScreen NewScreen)
{
	if (bMenuUITeardown
		|| GetNetMode() == NM_DedicatedServer
		|| !IsLocalController()
		|| NewScreen == ELBMainMenuScreen::None)
	{
		return;
	}

	DesiredScreen = NewScreen;
	ShowDesiredMenuScreen();
}

void ALB_MainMenuPlayerController::SubmitCodename(const FText& RawCodename)
{
	if (!IsLocalController() || bMenuUITeardown)
	{
		return;
	}

	const FString RawCodenameString = RawCodename.ToString();
	if (HasAuthority())
	{
		HandleCodenameSubmission_ServerOnly(RawCodenameString);
		return;
	}

	ServerSubmitCodename(RawCodenameString);
}

void ALB_MainMenuPlayerController::RequestStartHunt()
{
	if (!IsLocalListenHost())
	{
		UE_LOG(LogLBMainMenuPlayerController, Warning, TEXT("Remote start request rejected. Controller=%s"), *GetNameSafe(this));
		return;
	}

	ALB_MainMenuGameMode* MainMenuGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ALB_MainMenuGameMode>() : nullptr;
	if (!IsValid(MainMenuGameMode) || !MainMenuGameMode->TryStartHunt(this))
	{
		UE_LOG(LogLBMainMenuPlayerController, Verbose, TEXT("Start hunt request did not pass server policy."));
	}
}

bool ALB_MainMenuPlayerController::CanRequestStartHunt() const
{
	if (!IsLocalListenHost())
	{
		return false;
	}

	const ALB_MainMenuGameMode* MainMenuGameMode = GetWorld()
		? GetWorld()->GetAuthGameMode<ALB_MainMenuGameMode>()
		: nullptr;
	return IsValid(MainMenuGameMode) && MainMenuGameMode->CanStartHunt(this);
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
		CodenameWidget.Get(),
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
	CodenameWidget = nullptr;
	CharacterSelectWidget = nullptr;
	WaitingWidget = nullptr;
	DesiredScreen = ELBMainMenuScreen::None;
	VisibleScreen = ELBMainMenuScreen::None;
}

void ALB_MainMenuPlayerController::ShowDesiredMenuScreen()
{
	if (bMenuUITeardown || DesiredScreen == ELBMainMenuScreen::None)
	{
		return;
	}

	if (UUserWidget* ExistingWidget = GetMenuWidget(DesiredScreen))
	{
		UUserWidget* Widgets[] = {
			MainMenuWidget.Get(),
			CodenameWidget.Get(),
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
			ExistingWidget->AddToPlayerScreen(MenuWidgetZOrder);
		}
		ExistingWidget->SetVisibility(ESlateVisibility::Visible);
		VisibleScreen = DesiredScreen;
		ApplyMenuInputMode(ExistingWidget);
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
		UUserWidget* NewWidget = CreateWidget<UUserWidget>(this, LoadedClass);
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
	if (!IsValid(NewWidget))
	{
		UE_LOG(LogLBMainMenuPlayerController, Error, TEXT("Failed to create menu widget. Screen=%d"), static_cast<int32>(LoadedScreen));
		return;
	}

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
	case ELBMainMenuScreen::Codename:
		return CodenameWidget;
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
	case ELBMainMenuScreen::Codename:
		CodenameWidget = Widget;
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
	case ELBMainMenuScreen::Codename:
		return &CodenameWidgetClass;
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

void ALB_MainMenuPlayerController::HandleCodenameSubmission_ServerOnly(const FString& RawCodename)
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
	ClientReceiveCodenameSubmissionResult(Result, SanitizedCodename);
}

void ALB_MainMenuPlayerController::ServerSubmitCodename_Implementation(const FString& RawCodename)
{
	HandleCodenameSubmission_ServerOnly(RawCodename);
}

void ALB_MainMenuPlayerController::ClientReceiveCodenameSubmissionResult_Implementation(
	ELBCodenameSubmitResult Result,
	const FString& SanitizedCodename)
{
	if (Result == ELBCodenameSubmitResult::Accepted)
	{
		SetMenuScreen(ELBMainMenuScreen::Waiting);
	}

	OnCodenameSubmissionResult.Broadcast(Result, SanitizedCodename);
}
