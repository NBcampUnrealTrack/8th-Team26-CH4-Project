#include "GameMode/LB_MainMenuGameMode.h"

#include "GameState/LB_MainMenuGameState.h"
#include "Misc/PackageName.h"
#include "Player/LB_MainMenuPlayerController.h"
#include "Player/LB_PlayerState.h"
#include "System/Online/LB_OnlineSessionSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogLBMainMenuGameMode, Log, All);

namespace
{
	constexpr int32 LBMinCodenameLength = 2;
	constexpr int32 LBMaxCodenameLength = 12;
}

ALB_MainMenuGameMode::ALB_MainMenuGameMode()
{
	bUseSeamlessTravel = true;
	bStartPlayersAsSpectators = true;
	GameStateClass = ALB_MainMenuGameState::StaticClass();
	PlayerControllerClass = ALB_MainMenuPlayerController::StaticClass();
	PlayerStateClass = ALB_PlayerState::StaticClass();
	DefaultPawnClass = nullptr;
	HUDClass = nullptr;

	RaidMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(
		TEXT("/Game/LeftBehind/Maps/Main.Main")));
}

void ALB_MainMenuGameMode::BeginPlay()
{
	Super::BeginPlay();
	RefreshLobbySnapshot();
}

void ALB_MainMenuGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindRoomPhaseDelegate();
	Super::EndPlay(EndPlayReason);
}

void ALB_MainMenuGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	RefreshLobbySnapshot();

	UE_LOG(
		LogLBMainMenuGameMode,
		Log,
		TEXT("Player completed PostLogin. Player=%s"),
		*GetNameSafe(NewPlayer));
}

void ALB_MainMenuGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
	RefreshLobbySnapshot();
}

void ALB_MainMenuGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	(void)NewPlayer;
	// The menu intentionally has no Pawn. Skipping the base RestartPlayer call avoids
	// a failed spawn while keeping the PlayerState as an active non-spectator.
	// 이 콜백은 클라이언트가 현재 월드 로드를 마친 뒤에도 호출되므로 Ready 상태를 다시 계산한다.
	RefreshLobbySnapshot();
}

bool ALB_MainMenuGameMode::CanStartHunt(const APlayerController* RequestingController) const
{
	if (!HasAuthority()
		|| bTravelInProgress
		|| !IsValid(RequestingController)
		|| !RequestingController->HasAuthority()
		|| !RequestingController->IsLocalController())
	{
		return false;
	}

	const ENetMode NetMode = GetNetMode();
	if (NetMode != NM_ListenServer && NetMode != NM_Standalone)
	{
		return false;
	}

	int32 ConnectedPlayers = 0;
	int32 ConfirmedPlayers = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PlayerController = It->Get();
		const ALB_PlayerState* PlayerState = IsValid(PlayerController)
			? PlayerController->GetPlayerState<ALB_PlayerState>()
			: nullptr;
		if (!IsValid(PlayerState) || PlayerState->IsOnlyASpectator())
		{
			continue;
		}

		++ConnectedPlayers;
		ConfirmedPlayers += PlayerState->IsCodenameConfirmed() ? 1 : 0;
	}

	FString RaidPackageName;
	return ConnectedPlayers >= GetMinPlayersToStart()
		&& ConfirmedPlayers == ConnectedPlayers
		&& AreAllActivePlayersLoaded()
		&& GetRaidMapPackageName(RaidPackageName);
}

bool ALB_MainMenuGameMode::TryStartHunt(APlayerController* RequestingController)
{
	if (!CanStartHunt(RequestingController))
	{
		UE_LOG(
			LogLBMainMenuGameMode,
			Verbose,
			TEXT("Start hunt rejected. Requester=%s Authority=%d Local=%d"),
			*GetNameSafe(RequestingController),
			IsValid(RequestingController) && RequestingController->HasAuthority() ? 1 : 0,
			IsValid(RequestingController) && RequestingController->IsLocalController() ? 1 : 0);
		return false;
	}

	FString RaidPackageName;
	if (!GetRaidMapPackageName(RaidPackageName))
	{
		return false;
	}

	ULB_OnlineSessionSubsystem* OnlineSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<ULB_OnlineSessionSubsystem>()
		: nullptr;
	if (IsValid(OnlineSubsystem) && OnlineSubsystem->IsInRoom())
	{
		if (!OnlineSubsystem->IsRoomHost())
		{
			UE_LOG(LogLBMainMenuGameMode, Warning, TEXT("Only the EOS room host may lock and start the raid."));
			return false;
		}

		bTravelInProgress = true;
		RefreshLobbySnapshot();
		OnlineSubsystem->OnRoomPhaseUpdateComplete.AddUniqueDynamic(
			this,
			&ThisClass::HandleRoomPhaseUpdateComplete);
		if (!OnlineSubsystem->LockRoomForRaid())
		{
			UnbindRoomPhaseDelegate();
			bTravelInProgress = false;
			RefreshLobbySnapshot();
			UE_LOG(LogLBMainMenuGameMode, Error, TEXT("EOS room lock request could not be started."));
			return false;
		}

		UE_LOG(LogLBMainMenuGameMode, Log, TEXT("Waiting for EOS room lock before raid travel."));
		return true;
	}

	return StartRaidTravel();
}

ELBCodenameSubmitResult ALB_MainMenuGameMode::TryConfirmCodename(
	ALB_MainMenuPlayerController* RequestingController,
	const FString& RawCodename,
	FString& OutSanitizedCodename)
{
	OutSanitizedCodename.Reset();
	if (!HasAuthority() || !IsValid(RequestingController) || RequestingController->GetWorld() != GetWorld())
	{
		return ELBCodenameSubmitResult::NotInLobby;
	}

	if (bTravelInProgress)
	{
		return ELBCodenameSubmitResult::TravelInProgress;
	}

	ALB_PlayerState* PlayerState = RequestingController->GetPlayerState<ALB_PlayerState>();
	if (!IsValid(PlayerState) || PlayerState->IsOnlyASpectator())
	{
		return ELBCodenameSubmitResult::NotInLobby;
	}

	const ELBCodenameSubmitResult ValidationResult = ValidateCodename(RawCodename, OutSanitizedCodename);
	if (ValidationResult != ELBCodenameSubmitResult::Accepted)
	{
		return ValidationResult;
	}

	PlayerState->SetPlayerName(OutSanitizedCodename);
	PlayerState->SetCodenameConfirmed_ServerOnly(true);
	RefreshLobbySnapshot();

	UE_LOG(
		LogLBMainMenuGameMode,
		Log,
		TEXT("Codename confirmed. PlayerState=%s PlayerName=%s"),
		*GetNameSafe(PlayerState),
		*OutSanitizedCodename);
	return ELBCodenameSubmitResult::Accepted;
}

ELBCodenameSubmitResult ALB_MainMenuGameMode::ValidateCodename(
	const FString& RawCodename,
	FString& OutSanitizedCodename)
{
	OutSanitizedCodename = RawCodename.TrimStartAndEnd();
	if (OutSanitizedCodename.Len() < LBMinCodenameLength)
	{
		return ELBCodenameSubmitResult::TooShort;
	}

	if (OutSanitizedCodename.Len() > LBMaxCodenameLength)
	{
		return ELBCodenameSubmitResult::TooLong;
	}

	for (const TCHAR Character : OutSanitizedCodename)
	{
		if (Character == TEXT('\n')
			|| Character == TEXT('\r')
			|| Character == TEXT('\t')
			|| FChar::IsControl(Character))
		{
			return ELBCodenameSubmitResult::InvalidCharacters;
		}
	}

	return ELBCodenameSubmitResult::Accepted;
}

void ALB_MainMenuGameMode::RefreshLobbySnapshot()
{
	if (!HasAuthority())
	{
		return;
	}

	if (ALB_MainMenuGameState* MainMenuGameState = GetMainMenuGameState())
	{
		MainMenuGameState->SetMainMenuSnapshot_ServerOnly(BuildLobbySnapshot(true));
	}
}

FLBMainMenuSnapshot ALB_MainMenuGameMode::BuildLobbySnapshot(bool bAdvanceRevision)
{
	FLBMainMenuSnapshot Snapshot;
	Snapshot.MinPlayersToStart = GetMinPlayersToStart();

	FString RaidPackageName;
	if (GetRaidMapPackageName(RaidPackageName))
	{
		Snapshot.TargetMapName = FName(*FPackageName::GetShortName(RaidPackageName));
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		const ALB_PlayerState* PlayerState = IsValid(PlayerController)
			? PlayerController->GetPlayerState<ALB_PlayerState>()
			: nullptr;
		if (!IsValid(PlayerState) || PlayerState->IsOnlyASpectator())
		{
			continue;
		}

		++Snapshot.ConnectedPlayers;
		Snapshot.ConfirmedPlayers += PlayerState->IsCodenameConfirmed() ? 1 : 0;
	}

	if (bTravelInProgress)
	{
		Snapshot.Phase = ELBMainMenuPhase::Traveling;
	}
	else if (Snapshot.ConnectedPlayers >= Snapshot.MinPlayersToStart
		&& Snapshot.ConfirmedPlayers == Snapshot.ConnectedPlayers
		&& AreAllActivePlayersLoaded())
	{
		Snapshot.Phase = ELBMainMenuPhase::Ready;
	}
	else
	{
		Snapshot.Phase = ELBMainMenuPhase::CollectingNames;
	}

	if (bAdvanceRevision)
	{
		SnapshotRevision = SnapshotRevision == MAX_int32 ? 1 : SnapshotRevision + 1;
	}
	Snapshot.Revision = SnapshotRevision;
	return Snapshot;
}

bool ALB_MainMenuGameMode::AreAllActivePlayersLoaded() const
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PlayerController = It->Get();
		const APlayerState* PlayerState = IsValid(PlayerController) ? PlayerController->PlayerState.Get() : nullptr;
		if (IsValid(PlayerState)
			&& !PlayerState->IsOnlyASpectator()
			&& !PlayerController->HasClientLoadedCurrentWorld())
		{
			return false;
		}
	}

	return true;
}

bool ALB_MainMenuGameMode::GetRaidMapPackageName(FString& OutPackageName) const
{
	OutPackageName.Reset();
	const FSoftObjectPath RaidMapPath = RaidMap.ToSoftObjectPath();
	if (RaidMapPath.IsNull() || !RaidMapPath.IsValid())
	{
		return false;
	}

	OutPackageName = FPackageName::ObjectPathToPackageName(RaidMapPath.GetAssetPathString());
	FText InvalidReason;
	if (!FPackageName::IsValidLongPackageName(OutPackageName, true, &InvalidReason))
	{
		UE_LOG(
			LogLBMainMenuGameMode,
			Error,
			TEXT("RaidMap has invalid package path. Path=%s Reason=%s"),
			*RaidMapPath.ToString(),
			*InvalidReason.ToString());
		OutPackageName.Reset();
		return false;
	}

	if (!FPackageName::DoesPackageExist(OutPackageName))
	{
		UE_LOG(LogLBMainMenuGameMode, Error, TEXT("RaidMap package does not exist. Package=%s"), *OutPackageName);
		OutPackageName.Reset();
		return false;
	}

	return true;
}

ALB_MainMenuGameState* ALB_MainMenuGameMode::GetMainMenuGameState() const
{
	return GetGameState<ALB_MainMenuGameState>();
}

bool ALB_MainMenuGameMode::StartRaidTravel()
{
	FString RaidPackageName;
	if (!GetRaidMapPackageName(RaidPackageName))
	{
		bTravelInProgress = false;
		RefreshLobbySnapshot();
		return false;
	}

	if (!bTravelInProgress)
	{
		bTravelInProgress = true;
		RefreshLobbySnapshot();
	}

	UWorld* World = GetWorld();
	if (!IsValid(World) || !World->ServerTravel(RaidPackageName, false))
	{
		bTravelInProgress = false;
		RefreshLobbySnapshot();
		UE_LOG(LogLBMainMenuGameMode, Error, TEXT("ServerTravel failed immediately. URL=%s"), *RaidPackageName);

		if (ULB_OnlineSessionSubsystem* OnlineSubsystem = GetGameInstance()
			? GetGameInstance()->GetSubsystem<ULB_OnlineSessionSubsystem>()
			: nullptr;
			IsValid(OnlineSubsystem) && OnlineSubsystem->IsRoomHost())
		{
			OnlineSubsystem->ReopenRoomAfterRaid();
		}
		return false;
	}

	UE_LOG(LogLBMainMenuGameMode, Log, TEXT("Starting seamless ServerTravel. URL=%s"), *RaidPackageName);
	return true;
}

void ALB_MainMenuGameMode::UnbindRoomPhaseDelegate()
{
	if (ULB_OnlineSessionSubsystem* OnlineSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<ULB_OnlineSessionSubsystem>()
		: nullptr)
	{
		OnlineSubsystem->OnRoomPhaseUpdateComplete.RemoveDynamic(
			this,
			&ThisClass::HandleRoomPhaseUpdateComplete);
	}
}

void ALB_MainMenuGameMode::HandleRoomPhaseUpdateComplete(
	bool bWasSuccessful,
	ELBRoomPhase Phase,
	const FText& ErrorMessage)
{
	UnbindRoomPhaseDelegate();
	if (!bTravelInProgress)
	{
		return;
	}

	if (!bWasSuccessful || Phase != ELBRoomPhase::InRaid)
	{
		bTravelInProgress = false;
		RefreshLobbySnapshot();
		UE_LOG(
			LogLBMainMenuGameMode,
			Error,
			TEXT("EOS room lock failed; raid travel was cancelled. Error=%s"),
			*ErrorMessage.ToString());
		return;
	}

	StartRaidTravel();
}
