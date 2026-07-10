// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Base/LB_BaseUserWidget.h"
#include "System/Raid/LBRaidTypes.h"
#include "LB_RaidScoreSlotWidget.generated.h"

// 결과 화면에서 플레이어 1명의 성과를 표시하는 슬롯

UCLASS()
class LEFTBEHIND_API ULB_RaidScoreSlotWidget : public ULB_BaseUserWidget
{
	GENERATED_BODY()
	
public:
	
	// 슬롯에 표시할 플레이어 결과를 설정
	UFUNCTION(BlueprintCallable, Category="LB|Raid")
	void SetPlayerResult(const FLBPlayerFinalResult& PlayerResult);
	
protected:

	// BP에서 실제 UI를 갱신
	UFUNCTION(BlueprintImplementableEvent, Category="LB|Raid")
	void BP_UpdatePlayerResult(const FLBPlayerFinalResult& PlayerResult);

};
