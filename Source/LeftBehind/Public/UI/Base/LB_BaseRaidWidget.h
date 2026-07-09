// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LB_BaseUserWidget.h"
#include "System/Raid/LBRaidTypes.h"
#include "LB_BaseRaidWidget.generated.h"

// Raid UI 공통 부모 클래스
// RaidGameState를 캐싱하고 Delegate를 관리 * Blueprint 이벤트로 전달

UCLASS(Abstract)
class LEFTBEHIND_API ULB_BaseRaidWidget : public ULB_BaseUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	
	UFUNCTION(BlueprintPure, Category="LB|Raid")
	ALB_RaidGameState* GetRaidGameState() const;
	
	// RaidGameState Delegate 3종
	UFUNCTION()
	void OnRaidStateChanged(ELBRaidState NewState);

	UFUNCTION()
	void OnBossHPChanged(float CurrentHP, float MaxHP);

	UFUNCTION()
	void OnRaidResultChanged(const FLBRaidResultData& ResultData);
	
	UFUNCTION()
	void OnRaidScoreboardChanged(const FLBRaidScoreboardData& ScoreboardData);
	
	// C++ 확장용
	virtual void HandleRaidStateChanged(ELBRaidState NewState);
	virtual void HandleBossHPChanged(float CurrentHP, float MaxHP);
	virtual void HandleRaidResultChanged(const FLBRaidResultData& ResultData);
	virtual void HandleRaidScoreboardChanged(const FLBRaidScoreboardData& ScoreboardData);
	
	// BP 확장용
	UFUNCTION(BlueprintImplementableEvent, Category="LB|Raid")
	void BP_OnRaidStateChanged(ELBRaidState NewState);

	UFUNCTION(BlueprintImplementableEvent, Category="LB|Raid")
	void BP_OnBossHPChanged(float CurrentHP, float MaxHP);

	UFUNCTION(BlueprintImplementableEvent, Category="LB|Raid")
	void BP_OnRaidResultChanged(const FLBRaidResultData& ResultData);
	
	UFUNCTION(BlueprintImplementableEvent, Category="LB|Raid")
	void BP_OnRaidScoreboardChanged(const FLBRaidScoreboardData& ScoreboardData);

private:
	void BindRaidGameState();
	void UnbindRaidGameState();
	void SyncCurrentRaidState();
	
protected:
	UPROPERTY(BlueprintReadOnly, Category="LB|Raid")
	TObjectPtr<ALB_RaidGameState> CachedRaidGameState;
};
