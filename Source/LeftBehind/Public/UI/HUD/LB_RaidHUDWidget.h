// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "../Base/LB_BaseRaidWidget.h"
#include "LB_RaidHUDWidget.generated.h"

// UI 상태 관리자 + 전체 조합 -> 최종 UI
// RaidState 감지 후, 보여줄 UI 결정 (대기->카운트다운->배틀->결과)
// 자식 위젯을 보여주고 숨기는 것만 담당할 것

class UOverlay;
class ULB_CountdownWidget;
class ULB_BattleHUDWidget;
class ULB_RaidResultWidget;

UCLASS()
class LEFTBEHIND_API ULB_RaidHUDWidget : public ULB_BaseRaidWidget
{
	GENERATED_BODY()
	
protected:

	virtual void NativeConstruct() override;

	// LB_RaidWidget에서 전달되는 이벤트
	virtual void HandleRaidStateChanged(ELBRaidState NewState) override;
	virtual void HandleRaidResultChanged(const FLBRaidResultData& ResultData) override;

	// 배틀 HUD
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<ULB_BattleHUDWidget> BattleHUD;

	// 카운트다운 오버레이
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<ULB_CountdownWidget> CountdownWidget;

	// 결과 팝업
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<ULB_RaidResultWidget> ResultWidget;

private:
	
	void ShowWaiting();
	void ShowCountdown();
	void ShowBattle();
	void ShowResult();
};
