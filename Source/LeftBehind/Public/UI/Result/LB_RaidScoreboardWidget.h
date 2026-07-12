// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "System/Raid/LBRaidTypes.h"
#include "UI/Base/LB_BaseRaidWidget.h"
#include "LB_RaidScoreboardWidget.generated.h"

// 레이드 종료 후 플레이어별 최종 성과를 보여주는 결과 화면
// BaseRaidWidget을 통해 RaidScoreboardData 이벤트 전달받음

class UHorizontalBox;
class ULB_RaidScoreSlotWidget;

UCLASS()
class LEFTBEHIND_API ULB_RaidScoreboardWidget : public ULB_BaseRaidWidget
{
	GENERATED_BODY()

public:
	ULB_RaidScoreboardWidget(const FObjectInitializer& ObjectInitializer);
	
protected:
	
	// ScoreboardData 변경 시 호출
	virtual void HandleRaidScoreboardChanged(const FLBRaidScoreboardData& ScoreboardData) override;

	// 현재 ScoreboardData로 슬롯 생성 및 갱신
	void RefreshScoreboard();
	
protected:

	// 플레이어 결과 슬롯이 들어갈 컨테이너
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> PlayerResultContainer;

	// 플레이어 결과 슬롯 BP
	UPROPERTY(EditDefaultsOnly, Category="LB|Raid")
	TSoftClassPtr<ULB_RaidScoreSlotWidget> RaidScoreSlotClass;

private:

	// 현재 표시 중인 결과 데이터
	FLBRaidScoreboardData CachedScoreboardData;
};
