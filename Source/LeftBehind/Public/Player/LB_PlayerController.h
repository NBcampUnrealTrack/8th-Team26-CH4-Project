// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LB_PlayerController.generated.h"

struct FGameplayTag;
struct FInputActionValue;
struct FStreamableHandle;

class UAbilitySystemComponent;
class UInputAction;
class UInputMappingContext;
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
	virtual void OnUnPossess() override;

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

	// 서버가 실제 공격 시도 시각을 보관해 hold 타이머와 단발 RPC가 동일한 rate gate를 공유한다.
	double LastPrimaryActivationServerTime = -1.0;
	bool bLocalPrimaryHeld = false;
	bool bServerPrimaryHeld = false;
	FTimerHandle ServerPrimaryRepeatTimerHandle;
	
	// 전투 전체 HUD다.
	// PlayerController는 이 위젯 하나만 화면에 붙이고,
	// 보스 HP / 플레이어 HP / 결과 UI는 내부 위젯들이 관리한다.
	UPROPERTY(EditDefaultsOnly, Category = "LB|UI|Raid")
	TSoftClassPtr<ULB_RaidHUDWidget> RaidHUDWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "LB|UI|Raid")
	int32 RaidHUDWidgetZOrder = 10;

	UPROPERTY(Transient)
	TObjectPtr<ULB_RaidHUDWidget> RaidHUDWidget;

	// 이 Controller가 실제로 추가한 컨텍스트만 기록해 다른 시스템이 소유한 매핑을 EndPlay에서 제거하지 않는다.
	UPROPERTY(Transient)
	TSet<TObjectPtr<UInputMappingContext>> AppliedInputMappingContexts;

	FTimerHandle RaidHUDInitRetryTimerHandle;
	TSharedPtr<FStreamableHandle> RaidHUDLoadHandle;
	bool bRaidHUDInitializationStopped = false;
	
	void Jump();
	void StopJumping();
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void PrimaryPressed();
	void PrimaryReleased();
	void SetPrimaryHeld_ServerOnly(bool bHeld);
	void HandlePrimaryRepeat_ServerOnly();
	void StopPrimaryRepeat_ServerOnly();
	bool TryActivatePrimary_ServerOnly();
	float GetSafePrimaryActivationInterval() const;
	bool IsAlive() const;

	void ApplyInputMappingContexts();
	void RemoveAppliedInputMappingContexts();
	bool ActivateAbility(const FGameplayTag& AbilityTag) const;
	void LogAbilityActivationFailure(const FGameplayTag& AbilityTag, const UAbilitySystemComponent* ASC) const;
	
	void InitializeRaidHUD();
	void ScheduleRaidHUDInitializationRetry();
	bool AreRaidHUDDependenciesReady() const;
	void RequestRaidHUDClassAsync();
	void HandleRaidHUDClassLoaded();
	void CancelRaidHUDClassLoad();
	void RemoveRaidHUD();
	
	// 눌림/뗌 두 번만 전송하고 연사 주기는 서버 타이머가 담당해 프레임 기반 RPC 폭증을 막는다.
	UFUNCTION(Server, Reliable)
	void ServerSetPrimaryHeld(bool bHeld);

	// 기존 BlueprintCallable 단발 API를 서버 권한으로 전달하는 태그 없는 전용 RPC다.
	UFUNCTION(Server, Reliable)
	void ServerRequestPrimaryAttack();
	
};
