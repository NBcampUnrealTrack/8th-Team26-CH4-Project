// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "../Base/LB_BaseUserWidget.h"
#include "LB_PlayerStatusWidget.generated.h"

// GameState에서 데이터 받는 보스HP, 타이머와 달리 
// 나(Local Player)의 ASC 직접 구독하고 갱신

class UAbilitySystemComponent;
class ULB_AttributeSet;

UCLASS()
class LEFTBEHIND_API ULB_PlayerStatusWidget : public ULB_BaseUserWidget
{
	GENERATED_BODY()
	
protected:

	// 플레이어 ASC의 Attribute 변경 이벤트 구독
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 플레이어의 ASC 찾아 HP/MP 변경 이벤트 구독
	void BindPlayerAttributes();
	void UnbindPlayerAttributes();

	// Attribute 변경 이벤트
	void OnHealthChanged(const struct FOnAttributeChangeData& Data);
	void OnManaChanged(const struct FOnAttributeChangeData& Data);
	
	// HP/MP 변경 시 BP에서 UI 갱신
	UFUNCTION(BlueprintImplementableEvent, Category="LB|Player Status")
	void BP_OnHealthChanged(float CurrentHP, float MaxHP);

	UFUNCTION(BlueprintImplementableEvent, Category="LB|Player Status")
	void BP_OnManaChanged(float CurrentMP, float MaxMP);

	UFUNCTION(BlueprintImplementableEvent, Category="LB|Player Status")
	void BP_UpdatePortrait(UTexture2D* Portrait);
	
	// 현재 Attribute 값으로 UI 갱신
	void RefreshStatus();
	
	void RefreshPortrait();
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="LB|Character")
	TObjectPtr<UDataTable> CharacterDataTable;

private:

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> CachedASC;
	
	UPROPERTY()
	TObjectPtr<ULB_AttributeSet> CachedAttributeSet;

	// Delegate 해제용 Handle
	FDelegateHandle HealthChangedHandle;
	FDelegateHandle ManaChangedHandle;
};
