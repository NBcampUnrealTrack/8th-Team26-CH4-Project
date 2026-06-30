//LBRaidTypes.h

#pragma once

#include "CoreMinimal.h"
#include "LBRaidTypes.generated.h"

UENUM(BlueprintType)
enum class ELBRaidState : uint8
{
	Waiting     UMETA(DisplayName="Waiting"),
	Countdown   UMETA(DisplayName="Countdown"),
	Battle      UMETA(DisplayName="Battle"),
	Result      UMETA(DisplayName="Result")
};

UENUM(BlueprintType)
enum class ELBRaidEndReason : uint8
{
	None        UMETA(DisplayName="None"),
	BossKilled  UMETA(DisplayName="BossKilled"),
	AllDead     UMETA(DisplayName="AllDead"),
	TimeOut     UMETA(DisplayName="TimeOut")
};

USTRUCT(BlueprintType)
struct FLBRaidResultData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bVictory = false;

	UPROPERTY(BlueprintReadOnly)
	ELBRaidEndReason EndReason = ELBRaidEndReason::None;

	UPROPERTY(BlueprintReadOnly)
	float ClearTimeSec = 0.f;

	UPROPERTY(BlueprintReadOnly)
	FName RankID = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	int32 PlayerDeaths = 0;

	UPROPERTY(BlueprintReadOnly)
	float BossRemainingHPOnFail = 0.f;
};