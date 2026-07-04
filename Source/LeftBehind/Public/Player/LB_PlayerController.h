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
class ALB_RaidGameState;
class ULB_AttributeWidget;
class ULB_RaidResultWidget;

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

	// 화면 상단에 띄울 보스 HP 위젯이다. 기본값은 WBP_HealthBar이고, 필요하면 전용 WBP로 교체할 수 있다.
	UPROPERTY(EditDefaultsOnly, Category = "LB|UI|Raid")
	TSubclassOf<ULB_AttributeWidget> BossHPWidgetClass;

	// 보스 HP 위젯의 화면상 크기다.
	UPROPERTY(EditDefaultsOnly, Category = "LB|UI|Raid")
	FVector2D BossHPWidgetSize = FVector2D(600.f, 28.f);

	// 화면 위쪽에서 얼마나 내려서 표시할지 정한다.
	UPROPERTY(EditDefaultsOnly, Category = "LB|UI|Raid")
	float BossHPWidgetTopOffset = 24.f;

	// 다른 UI보다 위에 보이도록 AddToViewport 순서를 지정한다.
	UPROPERTY(EditDefaultsOnly, Category = "LB|UI|Raid")
	int32 BossHPWidgetZOrder = 50;

	// true면 Battle 상태에서만 보스 HP를 보여준다.
	UPROPERTY(EditDefaultsOnly, Category = "LB|UI|Raid")
	bool bShowBossHPOnlyInBattle = true;

	// 보스 HP가 0이 되어 승리했을 때 각 클라이언트 화면 중앙에 띄울 결과 위젯이다.
	UPROPERTY(EditDefaultsOnly, Category = "LB|UI|Raid")
	TSubclassOf<ULB_RaidResultWidget> RaidVictoryWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "LB|UI|Raid")
	int32 RaidVictoryWidgetZOrder = 100;

	UPROPERTY(Transient)
	TObjectPtr<ULB_AttributeWidget> BossHPWidget;

	UPROPERTY(Transient)
	TObjectPtr<ULB_RaidResultWidget> RaidVictoryWidget;

	FTimerHandle BossHPWidgetBindRetryTimerHandle;
	
	void Jump();
	void StopJumping();
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Primary();
	void ApplyInputMappingContexts() const;
	bool ActivateAbility(const FGameplayTag& AbilityTag) const;
	void LogAbilityActivationFailure(const FGameplayTag& AbilityTag, const UAbilitySystemComponent* ASC) const;
	void InitializeBossHPWidget();
	bool BindBossHPWidgetToGameState();
	void RefreshBossHPWidgetLayout() const;
	void UpdateBossHPWidgetVisibility(ELBRaidState RaidState) const;
	void RefreshRaidVictoryWidget();
	void ShowRaidVictoryWidget(const FLBRaidResultData& ResultData);
	void HideRaidVictoryWidget() const;

	UFUNCTION()
	void HandleBossHPChanged(float CurrentHP, float MaxHP);

	UFUNCTION()
	void HandleRaidStateChanged(ELBRaidState NewState);

	UFUNCTION()
	void HandleRaidResultChanged(const FLBRaidResultData& ResultData);

	// 클라이언트 입력은 서버로 보내고, 실제 전투 판정과 HP 변경은 서버에서만 실행한다.
	UFUNCTION(Server, Reliable)
	void ServerActivateAbility(FGameplayTag AbilityTag);

	// 서버가 각 소유 클라이언트에게 "네 화면에 레이드 HUD를 만들어라"라고 요청한다.
	UFUNCTION(Client, Reliable)
	void ClientInitializeBossHPWidget();
};
