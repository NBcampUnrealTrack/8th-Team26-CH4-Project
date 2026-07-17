// Fill out your copyright notice in the Description page of Project Settings.


#include "GameMode/LB_CharacterSelectGameMode.h"
#include "Engine/DataTable.h"
#include "System/Character/LBCharacterTypes.h"
#include "UI/CharacterSelect/LB_CharacterPreview.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"
#include "Player/LB_PlayerState.h"
#include "Player/LB_MainMenuPlayerController.h"
#include "GameState/LB_CharacterSelectGameState.h"
#include "Misc/PackageName.h"

ALB_CharacterSelectGameMode::ALB_CharacterSelectGameMode()
{
	bUseSeamlessTravel = true;
	bStartPlayersAsSpectators = false;

	GameStateClass = ALB_CharacterSelectGameState::StaticClass();
	PlayerStateClass = ALB_PlayerState::StaticClass();

	DefaultPawnClass = nullptr;
	HUDClass = nullptr;

	RaidMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(
		TEXT("/Game/LeftBehind/Maps/Blockout/Lv_Boss1_Blockout.Lv_Boss1_Blockout")));
}

void ALB_CharacterSelectGameMode::BeginPlay()
{
	Super::BeginPlay();
	
	if (!HasAuthority())
	{
		return;
	}

	// 캐릭터 선택 맵에 들어올 때마다 모든 플레이어 선택 상태 초기화
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (ALB_PlayerState* LBPlayerState = Cast<ALB_PlayerState>(PlayerState))
		{
			LBPlayerState->ResetCharacterSelection_ServerOnly();
		}
	}

	GetTargetPoints();

	InitCharacterPreview();

	RefreshSnapshot();
}

void ALB_CharacterSelectGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	
	if (ALB_PlayerState* PS =
		NewPlayer->GetPlayerState<ALB_PlayerState>())
	{
		PS->ResetCharacterSelection_ServerOnly();
	}
	
	RefreshSnapshot();
}

void ALB_CharacterSelectGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
	
	RefreshSnapshot();
}

ELBCharacterSelectResult ALB_CharacterSelectGameMode::TrySelectCharacter(ALB_MainMenuPlayerController* RequestingController,
	ELBCharacterID CharacterID)
{
	if (!HasAuthority())
	{
		return ELBCharacterSelectResult::Failed;
	}

	ALB_PlayerState* PS =
		RequestingController
		? RequestingController->GetPlayerState<ALB_PlayerState>()
		: nullptr;

	if (!IsValid(PS))
	{
		return ELBCharacterSelectResult::Failed;
	}

	if (PS->IsCharacterReady())
	{
		return ELBCharacterSelectResult::AlreadyReady;
	}

	if (CharacterID == ELBCharacterID::None)
	{
		return ELBCharacterSelectResult::InvalidCharacter;
	}

	if (IsCharacterAlreadySelected(CharacterID, PS))
	{
		return ELBCharacterSelectResult::AlreadySelected;
	}

	PS->SetCharacterID_ServerOnly(CharacterID);

	RefreshSnapshot();
	
	return ELBCharacterSelectResult::Success;
}

bool ALB_CharacterSelectGameMode::TrySetCharacterReady(ALB_MainMenuPlayerController* RequestingController)
{
	if (!HasAuthority())
	{
		return false;
	}

	ALB_PlayerState* PS =
		RequestingController
		? RequestingController->GetPlayerState<ALB_PlayerState>()
		: nullptr;

	if (!IsValid(PS))
	{
		return false;
	}

	if (PS->GetCharacterID() == ELBCharacterID::None)
	{
		return false;
	}

	PS->SetCharacterReady_ServerOnly(true);

	

	if (AreAllPlayersReady() && CurrentPhase == ELBCharacterSelectPhase::Waiting)
	{
		SetPhase(ELBCharacterSelectPhase::AllReady);

		GetWorldTimerManager().SetTimer(
			TravelTimerHandle,
			this,
			&ThisClass::DelayedStartRaidTravel,
			2.f,
			false);
	}
	else
	{
		RefreshSnapshot();
	}

	return true;
}

bool ALB_CharacterSelectGameMode::TryCancelCharacterReady(ALB_MainMenuPlayerController* RequestingController)
{
	if (!HasAuthority())
	{
		return false;
	}

	ALB_PlayerState* PS =
		RequestingController
		? RequestingController->GetPlayerState<ALB_PlayerState>()
		: nullptr;

	if (!IsValid(PS))
	{
		return false;
	}

	PS->SetCharacterReady_ServerOnly(false);

	if (CurrentPhase != ELBCharacterSelectPhase::Traveling)
	{
		SetPhase(ELBCharacterSelectPhase::Waiting);

		GetWorldTimerManager().ClearTimer(TravelTimerHandle);

		bTravelStarted = false;
	}
	return true;
}

bool ALB_CharacterSelectGameMode::IsCharacterAlreadySelected(ELBCharacterID CharacterID,
	const ALB_PlayerState* IgnorePlayer) const
{
	if (CharacterID == ELBCharacterID::None)
	{
		return false;
	}

	if (!GameState)
	{
		return false;
	}

	for (APlayerState* PSBase : GameState->PlayerArray)
	{
		const ALB_PlayerState* PS = Cast<ALB_PlayerState>(PSBase);

		if (!IsValid(PS))
		{
			continue;
		}

		if (PS == IgnorePlayer)
		{
			continue;
		}

		if (PS->GetCharacterID() == CharacterID)
		{
			return true;
		}
	}

	return false;
}

bool ALB_CharacterSelectGameMode::CanStartRaid(const APlayerController* RequestingController) const
{
	if (!HasAuthority())
	{
		return false;
	}

	if (!IsValid(RequestingController))
	{
		return false;
	}

	return AreAllPlayersReady();
}

bool ALB_CharacterSelectGameMode::TryStartRaid(APlayerController* RequestingController)
{
	if (!CanStartRaid(RequestingController))
	{
		return false;
	}

	return StartRaidTravel();
}

void ALB_CharacterSelectGameMode::GetTargetPoints()
{
	TargetPoints.Empty();

	for (TActorIterator<ATargetPoint> It(GetWorld()); It; ++It)
	{
		TargetPoints.Add(*It);
	}

	TargetPoints.Sort([](
		const ATargetPoint& A,
		const ATargetPoint& B)
	{
		return A.GetName() < B.GetName();
	});
}

void ALB_CharacterSelectGameMode::InitCharacterPreview()
{
	if (!CharacterDataTable || !CharacterPreviewClass) return;

	CharacterPreviews.Empty();

	TArray<FName> RowNames;
	GetCharacterRows(RowNames);

	for (int32 Index = 0; Index < RowNames.Num(); Index++)
	{
		const FLBCharacterData* Data = CharacterDataTable->FindRow<FLBCharacterData>(
			RowNames[Index], TEXT("CharacterPreview"));
		if (!Data) continue;

		// TargetPoint 없으면 경고 후 스킵
		if (!TargetPoints.IsValidIndex(Index))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[CharacterSelectGameMode] TargetPoint 부족. Index: %d"), Index);
			continue;
		}

		FTransform SpawnTransform = TargetPoints[Index]->GetActorTransform();

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ALB_CharacterPreview* Preview = GetWorld()->SpawnActor<ALB_CharacterPreview>(
			CharacterPreviewClass,
			SpawnTransform,   // ← TargetPoint 위치에 스폰
			SpawnParams);

		if (!Preview) continue;

		Preview->Initialize(*Data);
		CharacterPreviews.Add(Preview);
	}
}

void ALB_CharacterSelectGameMode::GetCharacterRows(TArray<FName>& OutRows) const
{
	OutRows.Empty();

	if (!CharacterDataTable)
	{
		return;
	}

	OutRows = CharacterDataTable->GetRowNames();

	OutRows.Sort([](const FName& A, const FName& B)
	{
		return A.LexicalLess(B);
	});
}

bool ALB_CharacterSelectGameMode::GetRaidMapPackageName(FString& OutPackageName) const
{
	OutPackageName.Reset();

	const FSoftObjectPath RaidMapPath =
		RaidMap.ToSoftObjectPath();

	if (RaidMapPath.IsNull() || !RaidMapPath.IsValid())
	{
		return false;
	}

	OutPackageName = FPackageName::ObjectPathToPackageName(RaidMapPath.GetAssetPathString());

	return FPackageName::DoesPackageExist(OutPackageName);
}

bool ALB_CharacterSelectGameMode::StartRaidTravel()
{
	FString RaidPackageName;

	if (!GetRaidMapPackageName(RaidPackageName))
	{
		return false;
	}

	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return false;
	}

	World->ServerTravel(RaidPackageName, false);
	
	UE_LOG(
		LogTemp,
		Log,
		TEXT("[CharacterSelect] ServerTravel: %s"),
		*RaidPackageName);
	
	return true;
}

void ALB_CharacterSelectGameMode::SetPhase(ELBCharacterSelectPhase NewPhase)
{
	if (CurrentPhase == NewPhase)
	{
		return;
	}

	CurrentPhase = NewPhase;

	RefreshSnapshot();
}

void ALB_CharacterSelectGameMode::DelayedStartRaidTravel()
{
	if (bTravelStarted)
	{
		return;
	}

	if (!AreAllPlayersReady())
	{
		SetPhase(ELBCharacterSelectPhase::Waiting);
		return;
	}

	bTravelStarted = true;

	SetPhase(ELBCharacterSelectPhase::Traveling);

	if (!StartRaidTravel())
	{
		bTravelStarted = false;
		SetPhase(ELBCharacterSelectPhase::Waiting);
	}
}


void ALB_CharacterSelectGameMode::RefreshSnapshot()
{
	if (!HasAuthority())
	{
		return;
	}

	if (ALB_CharacterSelectGameState* GS = GetCharacterSelectGameState())
	{
		GS->SetSnapshot_ServerOnly(BuildSnapshot(true));
	}
}

FLBCharacterSelectSnapshot ALB_CharacterSelectGameMode::BuildSnapshot(bool bAdvanceRevision)
{
	FLBCharacterSelectSnapshot Snapshot;

	if (GameState)
	{
		for (APlayerState* PSBase : GameState->PlayerArray)
		{
			const ALB_PlayerState* PS = Cast<ALB_PlayerState>(PSBase);

			if (!IsValid(PS) || PS->IsOnlyASpectator())
			{
				continue;
			}

			FLBCharacterSelectPlayerInfo Info;

			Info.PlayerName = PS->GetPlayerName();
			Info.CharacterID = PS->GetCharacterID();
			Info.RoleType = PS->GetRoleType();
			Info.bReady = PS->IsCharacterReady();

			Snapshot.Players.Add(Info);
		}
	}

	Snapshot.bEveryoneReady = AreAllPlayersReady();
	Snapshot.Phase = CurrentPhase;

	if (bAdvanceRevision)
	{
		SnapshotRevision =
			SnapshotRevision == MAX_int32
			? 1
			: SnapshotRevision + 1;
	}

	Snapshot.Revision = SnapshotRevision;

	return Snapshot;
}

ALB_CharacterSelectGameState* ALB_CharacterSelectGameMode::GetCharacterSelectGameState() const
{
	return GetGameState<ALB_CharacterSelectGameState>();
}

bool ALB_CharacterSelectGameMode::AreAllPlayersReady() const
{
	if (!GameState)
	{
		return false;
	}

	bool bHasPlayers = false;

	for (APlayerState* PSBase : GameState->PlayerArray)
	{
		const ALB_PlayerState* PS = Cast<ALB_PlayerState>(PSBase);

		if (!IsValid(PS) || PS->IsOnlyASpectator())
		{
			continue;
		}

		bHasPlayers = true;

		if (!PS->IsCharacterReady())
		{
			return false;
		}
	}

	return bHasPlayers;
}
