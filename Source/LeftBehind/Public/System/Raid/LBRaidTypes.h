// LBRaidTypes.h

#pragma once

#include "CoreMinimal.h"
#include "../Character/LBCharacterTypes.h"
#include "LBRaidTypes.generated.h"

// 네트워크/세이브/Blueprint에 저장되는 숫자의 의미가 바뀌지 않도록 기존 ordinal을 명시한다.
UENUM(BlueprintType)
enum class ELBRaidState : uint8
{
	// 레이드 시작 전 대기 상태. 플레이어 입장/초기화를 기다릴 때 사용한다.
	Waiting = 0     UMETA(DisplayName="Waiting"),
	// 전투 시작 전 카운트다운 상태. UI는 CountdownEndServerTime 기준으로 남은 시간을 표시한다.
	Countdown = 1   UMETA(DisplayName="Countdown"),
	// 보스가 스폰되고 제한 시간이 흐르는 실제 전투 상태.
	Battle = 2      UMETA(DisplayName="Battle"),
	// 승패가 확정되어 결과 UI와 보상/로그 처리를 진행하는 상태.
	Result = 3      UMETA(DisplayName="Result")
};

UENUM(BlueprintType)
enum class ELBRaidEndReason : uint8
{
	// 아직 레이드가 종료되지 않았거나 종료 사유가 정해지지 않은 기본값.
	None = 0        UMETA(DisplayName="None"),
	// 보스 체력이 0이 되어 승리한 경우.
	BossKilled = 1  UMETA(DisplayName="BossKilled"),
	// 모든 플레이어가 사망해 패배한 경우.
	AllDead = 2     UMETA(DisplayName="AllDead"),
	// 제한 시간이 끝날 때까지 보스를 처치하지 못해 패배한 경우.
	TimeOut = 3     UMETA(DisplayName="TimeOut")
};

USTRUCT(BlueprintType)
struct FLBRaidResultData
{
	GENERATED_BODY()

	// true면 클리어 성공, false면 패배 또는 중단 결과로 취급한다.
	UPROPERTY(BlueprintReadOnly)
	bool bVictory = false;

	// 결과 화면과 로그에서 사용할 레이드 종료 사유.
	UPROPERTY(BlueprintReadOnly)
	ELBRaidEndReason EndReason = ELBRaidEndReason::None;

	// 전투 시작 시점부터 종료까지 걸린 시간. 랭크 계산의 기준이 된다.
	UPROPERTY(BlueprintReadOnly)
	float ClearTimeSec = 0.f;

	// 랭크 데이터 테이블에서 매칭된 랭크 ID. 패배 시에는 NAME_None이 된다.
	UPROPERTY(BlueprintReadOnly)
	FName RankID = NAME_None;

	// 레이드 동안 누적된 전체 플레이어 사망 횟수.
	UPROPERTY(BlueprintReadOnly)
	int32 PlayerDeaths = 0;

	// 실패했을 때 남아 있던 보스 HP. 승리 시에는 0으로 기록한다.
	UPROPERTY(BlueprintReadOnly)
	float BossRemainingHPOnFail = 0.f;
};


// 플레이어 한 명의 최종 성과
USTRUCT(BlueprintType)
struct FLBPlayerFinalResult
{
	GENERATED_BODY()

	// 플레이어 이름
	UPROPERTY(BlueprintReadOnly)
	FText PlayerName;

	// 선택한 캐릭터
	UPROPERTY(BlueprintReadOnly)
	ELBCharacterID CharacterID = ELBCharacterID::None;

	// 역할
	UPROPERTY(BlueprintReadOnly)
	ELBRoleType RoleType = ELBRoleType::DPS;

	// 누적 피해량
	UPROPERTY(BlueprintReadOnly)
	float TotalDamageDealt = 0.f;

	// 누적 회복량
	UPROPERTY(BlueprintReadOnly)
	float TotalHealingDone = 0.f;

	// 사망 횟수
	UPROPERTY(BlueprintReadOnly)
	int32 DeathCount = 0;

	// MVP 여부
	UPROPERTY(BlueprintReadOnly)
	bool bIsMVP = false;
};


// 전체 결과 화면(Scoreboard)에서 사용하는 데이터
USTRUCT(BlueprintType)
struct FLBRaidScoreboardData
{
	GENERATED_BODY()

	// true면 클리어 성공, false면 패배 또는 중단 결과로 취급한다.
	UPROPERTY(BlueprintReadOnly)
	bool bVictory = false;
	
	// 결과 화면과 로그에서 사용할 레이드 종료 사유.
	UPROPERTY(BlueprintReadOnly)
	ELBRaidEndReason EndReason = ELBRaidEndReason::None;
	
	// 클리어 시간
	UPROPERTY(BlueprintReadOnly)
	float ClearTimeSec = 0.f;

	// 최종 랭크
	UPROPERTY(BlueprintReadOnly)
	FName RankID = NAME_None;
	
	// 플레이어 결과 목록
	UPROPERTY(BlueprintReadOnly)
	TArray<FLBPlayerFinalResult> PlayerResults;
};
