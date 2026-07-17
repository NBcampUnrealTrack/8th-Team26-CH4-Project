#pragma once

#include "CoreMinimal.h"
#include "System/Character/LBCharacterTypes.h"
#include "LBCharacterSelectTypes.generated.h"

// 캐릭터 선택 화면에서 플레이어 한 명의 상태.
USTRUCT(BlueprintType)
struct FLBCharacterSelectPlayerInfo
{
	GENERATED_BODY()

	// 코드네임(PlayerState::PlayerName)
	UPROPERTY(BlueprintReadOnly)
	FString PlayerName;

	// 선택한 캐릭터
	UPROPERTY(BlueprintReadOnly)
	ELBCharacterID CharacterID = ELBCharacterID::None;

	// Ready 여부
	UPROPERTY(BlueprintReadOnly)
	bool bReady = false;
	
	UPROPERTY(BlueprintReadOnly)
	ELBRoleType RoleType = ELBRoleType::DPS;
};


UENUM(BlueprintType)
enum class ELBCharacterSelectPhase : uint8
{
	Waiting = 0,
	AllReady = 1,
	Traveling = 2
};

// 캐릭터 선택 화면 전체 상태.
// GameState가 복제하는 데이터.
USTRUCT(BlueprintType)
struct FLBCharacterSelectSnapshot
{
	GENERATED_BODY()

	// 현재 참가자 목록
	UPROPERTY(BlueprintReadOnly)
	TArray<FLBCharacterSelectPlayerInfo> Players;

	// 전원 Ready 여부
	UPROPERTY(BlueprintReadOnly)
	bool bEveryoneReady = false;

	// 변경 감지용 Revision
	UPROPERTY(BlueprintReadOnly)
	int32 Revision = 0;
	
	UPROPERTY(BlueprintReadOnly)
	ELBCharacterSelectPhase Phase =
		ELBCharacterSelectPhase::Waiting;
};

UENUM(BlueprintType)
enum class ELBCharacterSelectResult : uint8
{
	Success = 0,
	AlreadySelected = 1,
	AlreadyReady = 2,
	InvalidCharacter = 3,
	Failed = 4
};
