// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "GameplayTagContainer.h"
#include "System/Raid/LBRaidTypes.h"
#include "LB_PlayerController.generated.h"

class UAbilitySystemComponent;
class ULB_RaidHUDWidget;

/**
 * 
 */
UCLASS()
class LEFTBEHIND_API ALB_PlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ALB_PlayerController();
	
protected:
	virtual void SetupInputComponent() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void AcknowledgePossession(APawn* P) override;

	// 블루프린트 입력 그래프가 남아 있어도 이 함수만 호출하면 C++의 서버 권한 공격 경로를 탄다.
	UFUNCTION(BlueprintCallable, Category = "LB|Input|Abilities")
	void RequestPrimaryAttack();
	
private: 
	UPROPERTY(EditDefaultsOnly, Category = "LB|Input|Movement")
	TArray<TObjectPtr<UInputMappingContext>> InputMappingContexts;
	
	UPROPERTY(EditDefaultsOnly, Category = "LB|Input|Movement")
	TObjectPtr<UInputAction> JumpAction;
	
	UPROPERTY(EditDefaultsOnly, Category = "LB|Input|Movement")
	TObjectPtr<UInputAction> MoveAction;
	
	UPROPERTY(EditDefaultsOnly, Category = "LB|Input|Movement")
	TObjectPtr<UInputAction> LookAction;
	
	UPROPERTY(EditDefaultsOnly, Category = "LB|Input|Abilities")
	TObjectPtr<UInputAction> PrimaryAction;

	// 꾹 눌러 연사할 때 몽타주가 매 프레임 끊기지 않도록 두는 최소 재발동 간격이다.
	UPROPERTY(EditDefaultsOnly, Category = "LB|Input|Abilities", meta = (ClampMin = "0.01"))
	float PrimaryActivationInterval = 0.3f;

	float LastPrimaryActivationTime = -1.f;
	
	// 전투 전체 HUD다.
	// PlayerController는 이 위젯 하나만 화면에 붙이고,
	// 보스 HP / 플레이어 HP / 결과 UI는 내부 위젯들이 관리한다.
	UPROPERTY(EditDefaultsOnly, Category = "LB|UI|Raid")
	TSubclassOf<ULB_RaidHUDWidget> RaidHUDWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "LB|UI|Raid")
	int32 RaidHUDWidgetZOrder = 10;

	UPROPERTY(Transient)
	TObjectPtr<ULB_RaidHUDWidget> RaidHUDWidget;
	
	FTimerHandle RaidHUDInitRetryTimerHandle;
	
	
	void Jump();
	void StopJumping();
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Primary();
	void ApplyInputMappingContexts() const;
	bool ActivateAbility(const FGameplayTag& AbilityTag) const;
	void LogAbilityActivationFailure(const FGameplayTag& AbilityTag, const UAbilitySystemComponent* ASC) const;
	
	void InitializeRaidHUD();
	void RemoveRaidHUD();
	
	// 클라이언트 입력은 서버로 보내고, 실제 전투 판정과 HP 변경은 서버에서만 실행한다.
	UFUNCTION(Server, Reliable)
	void ServerActivateAbility(FGameplayTag AbilityTag);
	
	UFUNCTION(Client, Reliable)
	void ClientInitializeRaidHUD();
	
};
