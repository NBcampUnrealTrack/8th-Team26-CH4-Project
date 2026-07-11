#pragma once

#include "CoreMinimal.h"
#include "LBMainMenuTypes.generated.h"

UENUM(BlueprintType)
enum class ELBMainMenuScreen : uint8
{
	None = 0 UMETA(Hidden),
	Main = 1 UMETA(DisplayName="Main"),
	Codename = 2 UMETA(DisplayName="Codename"),
	CharacterSelect = 3 UMETA(DisplayName="Character Select"),
	Waiting = 4 UMETA(DisplayName="Waiting"),
	Multiplayer = 5 UMETA(DisplayName="Multiplayer")
};

UENUM(BlueprintType)
enum class ELBMainMenuPhase : uint8
{
	CollectingNames = 0 UMETA(DisplayName="Collecting Names"),
	Ready = 1 UMETA(DisplayName="Ready"),
	Traveling = 2 UMETA(DisplayName="Traveling")
};

UENUM(BlueprintType)
enum class ELBCodenameSubmitResult : uint8
{
	Accepted = 0 UMETA(DisplayName="Accepted"),
	TooShort = 1 UMETA(DisplayName="Too Short"),
	TooLong = 2 UMETA(DisplayName="Too Long"),
	InvalidCharacters = 3 UMETA(DisplayName="Invalid Characters"),
	NotInLobby = 4 UMETA(DisplayName="Not In Lobby"),
	TravelInProgress = 5 UMETA(DisplayName="Travel In Progress")
};

USTRUCT(BlueprintType)
struct FLBMainMenuSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="LB|MainMenu")
	int32 ConnectedPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category="LB|MainMenu")
	int32 ConfirmedPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category="LB|MainMenu")
	int32 MinPlayersToStart = 1;

	UPROPERTY(BlueprintReadOnly, Category="LB|MainMenu")
	FName TargetMapName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category="LB|MainMenu")
	ELBMainMenuPhase Phase = ELBMainMenuPhase::CollectingNames;

	// 이름 재확정처럼 카운트가 같아도 대기실이 PlayerArray를 다시 그릴 수 있게 한다.
	UPROPERTY(BlueprintReadOnly, Category="LB|MainMenu")
	int32 Revision = 0;

	bool operator==(const FLBMainMenuSnapshot& Other) const
	{
		return ConnectedPlayers == Other.ConnectedPlayers
			&& ConfirmedPlayers == Other.ConfirmedPlayers
			&& MinPlayersToStart == Other.MinPlayersToStart
			&& TargetMapName == Other.TargetMapName
			&& Phase == Other.Phase
			&& Revision == Other.Revision;
	}

	bool operator!=(const FLBMainMenuSnapshot& Other) const
	{
		return !(*this == Other);
	}
};
