// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Base/LB_BaseUserWidget.h"
#include "System/Character/LBCharacterTypes.h"
#include "LB_PartyMemberSlotWidget.generated.h"

// 파티원 한 명의 상태를 표시하는 슬롯 위젯
// PlayerState에서 이름, 역할, 사망 상태 + ASC에서 Attribute(HP) 정보를 가져옴
// 파티원 전체 목록 관리는 LB_PartyStatusWidget 에서 함

class ALB_PlayerState;
class ULB_AbilitySystemComponent;
class ULB_AttributeSet;
struct FOnAttributeChangeData;

class ALB_PlayerState;

UCLASS()
class LEFTBEHIND_API ULB_PartyMemberSlotWidget : public ULB_BaseUserWidget
{
	GENERATED_BODY()
	
public:

	// 표시할 PlayerState 지정
	UFUNCTION(BlueprintCallable, Category="LB|Party")
	void SetPlayerState(ALB_PlayerState* InPlayerState);

protected:

	virtual void NativeDestruct() override;

	// PlayerState 변경 이벤트 구독
	void BindPlayerState();
	void UnbindPlayerState();

	// ASC 찾아 HP 변경 이벤트 구독
	void BindAttributes();
	void UnbindAttributes();
	

	// PlayerState 변경 이벤트
	UFUNCTION()
	void OnRoleChanged(ELBRoleType NewRoleType);

	UFUNCTION()
	void OnDeadStateChanged(bool bIsDead);
	
	// Attribute 변경 이벤트
	void OnHealthChanged(const FOnAttributeChangeData& Data);
	void OnMaxHealthChanged(const FOnAttributeChangeData& Data);

	UFUNCTION()
	void OnAttributesInitialized();
	
	// BP에 전체 정보 전달
	UFUNCTION(BlueprintImplementableEvent, Category="LB|Party")
	void BP_UpdatePartyMember(
		const FText& PlayerName,
		ELBRoleType RoleType,
		float CurrentHP,
		float MaxHP,
		bool bDead);


	void RefreshAll();
	
private:

	UPROPERTY()
	TObjectPtr<ALB_PlayerState> CachedPlayerState;

	UPROPERTY()
	TObjectPtr<ULB_AbilitySystemComponent> CachedASC;

	UPROPERTY()
	TObjectPtr<ULB_AttributeSet> CachedAttributeSet;

	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;
};
