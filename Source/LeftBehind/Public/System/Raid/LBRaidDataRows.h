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

	// 데이터 테이블에서 보스를 구분하기 위한 식별자. 행 이름과 별도로 게임 로직에서 참조할 수 있다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName BossID = NAME_None;

	// 실제로 스폰할 보스 블루프린트/클래스. SoftClass라 필요할 때 동기 로드한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftClassPtr<ALBRaidBossBase> BossClass;

	// 보스의 최대 체력. 스폰 후 CurrentHP도 이 값으로 초기화한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float MaxHP = 10000.f;

	// 보스 방어력 값. 현재 기본 데미지 계산에는 직접 쓰이지 않지만 난이도 데이터로 보관한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float DEF = 50.f;

	// 해당 보스 전투의 제한 시간. GameState에 복제되어 UI 타이머 기준이 된다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float TimeLimitSec = 300.f;

	// 2페이즈 진입 기준 체력 비율. 보스 패턴 확장 시 사용할 수 있는 데이터다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Phase2ThresholdRatio = 0.5f;
};

USTRUCT(BlueprintType)
struct FLBRankDataRow : public FTableRowBase
{
	GENERATED_BODY()

	// 결과에 기록될 랭크 식별자. 예: Rank_S, Rank_A.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName RankID = NAME_None;

	// 이 시간 이하로 클리어하면 해당 랭크 후보가 된다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float ClearTimeSec = 300.f;

	// 결과 화면에 표시할 랭크명/칭호 텍스트.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Title;

	// 랭크에 연결할 보상 데이터 ID.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName RewardID = NAME_None;
};
