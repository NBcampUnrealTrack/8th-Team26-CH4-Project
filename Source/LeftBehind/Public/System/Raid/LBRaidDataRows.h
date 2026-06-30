//LBRaidDataRows.h

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "System/Raid/LBRaidBossBase.h"
#include "LBRaidDataRows.generated.h"

USTRUCT(BlueprintType)
struct FLBBossStatsRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName BossID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftClassPtr<ALBRaidBossBase> BossClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float MaxHP = 10000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float DEF = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float TimeLimitSec = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Phase2ThresholdRatio = 0.5f;
};

USTRUCT(BlueprintType)
struct FLBRankDataRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName RankID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float ClearTimeSec = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName RewardID = NAME_None;
};