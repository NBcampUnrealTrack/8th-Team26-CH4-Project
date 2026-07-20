// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "System/Raid/LBRaidTypes.h"
#include "LB_PlayerController.generated.h"

struct FGameplayTag;
struct FInputActionValue;
struct FStreamableHandle;

class ALB_RaidGameState;
class UAbilitySystemComponent;
class UInputAction;
class UInputMappingContext;
class ULevelSequencePlayer;
class ULB_RaidHUDWidget;
class ULB_RaidPauseMenuWidget;

/**
 * 
 */
UCLASS()
class LEFTBEHIND_API ALB_PlayerController : public APlayerController
{
	GENERATED_BODY()

#if WITH_DEV_AUTOMATION_TESTS
	friend class FLBRaidReturnToMenuContractTest;
	friend class FLBRaidPauseMenuContractTest;
#endif

public:
	ALB_PlayerController();
	void PlayRaidIntroForOwningClient();

	// 원격 클라이언트 RPC 없이 이동을 시작할 수 있는 로컬 Listen Host/Standalone인지 반환한다.
	UFUNCTION(BlueprintPure, Category = "LB|Raid|Network")
	bool IsLocalListenHost() const;

	UFUNCTION(BlueprintPure, Category = "LB|Raid|Travel")
	bool CanRequestReturnToMainMenu() const;

	// 결과 버튼이 실패 시 다시 활성화할 수 있도록 실제 이동 시작 여부를 반환한다.
	UFUNCTION(BlueprintCallable, Category = "LB|Raid|Travel")
	bool RequestReturnToMainMenu();

	// Waiting/Countdown/Battle에서 로컬 ESC 메뉴를 열거나 닫는다.
	UFUNCTION(BlueprintCallable, Category = "LB|Raid|Pause")
	void TogglePauseMenu();

	// 계속하기 버튼과 Result 전환이 공유하는 단일 닫기 경로다.
	UFUNCTION(BlueprintCallable, Category = "LB|Raid|Pause")
	void ClosePauseMenu();

	UFUNCTION(BlueprintPure, Category = "LB|Raid|Pause")
	bool IsPauseMenuOpen() const { return bPauseMenuOpen; }

	// 활성 레이드를 중단하고 기존 EOS 파티와 함께 대기실로 돌아갈 수 있는지 확인한다.
	UFUNCTION(BlueprintPure, Category = "LB|Raid|Travel")
	bool CanRequestAbortRaidToRoom() const;

	UFUNCTION(BlueprintCallable, Category = "LB|Raid|Travel")
	bool RequestAbortRaidToRoom();

	// 종료 확인 UI를 통과한 뒤에만 호출되는 실제 프로세스 종료 경로다.
	UFUNCTION(BlueprintCallable, Category = "LB|Raid|Pause")
	void ConfirmQuitGame();
	
protected:
	virtual void SetupInputComponent() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void ReceivedPlayer() override;
	virtual void BeginPlayingState() override;
	virtual void OnRep_PlayerState() override;
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
	
	UPROPERTY(EditDefaultsOnly, Category = "LB|Input|Abilities")
	TObjectPtr<UInputAction> SecondaryAction;

	// 꾹 눌러 연사할 때 몽타주가 매 프레임 끊기지 않도록 두는 최소 재발동 간격이다.
	UPROPERTY(EditDefaultsOnly, Category = "LB|Input|Abilities", meta = (ClampMin = "0.01"))
	float PrimaryActivationInterval = 0.3f;

	// 우클릭 스킬도 몽타주가 끝나기 전에 재발동되지 않도록 두는 최소 간격이다.
	UPROPERTY(EditDefaultsOnly, Category = "LB|Input|Abilities", meta = (ClampMin = "0.01"))
	float SecondaryActivationInterval = 0.3f;

	// 서버가 실제 공격 시도 시각을 보관해 hold 타이머와 단발 RPC가 동일한 rate gate를 공유한다.
	double LastPrimaryActivationServerTime = -1.0;
	// 서버가 마지막으로 우클릭 스킬을 시도한 시각을 기록한다.
	double LastSecondaryActivationServerTime = -1.0;
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
	// HUD를 생성할 때 소유하던 Pawn. 컷씬 중 nullptr로 먼저 만든 HUD는 Possess 후 재생성한다.
	TWeakObjectPtr<APawn> RaidHUDInitializedPawn;

	// 전투 HUD와 독립된 전체 화면 액션 메뉴다. 작은 UI만 로컬 클라이언트에서 비동기 로드한다.
	UPROPERTY(EditDefaultsOnly, Category = "LB|UI|Raid|Pause")
	TSoftClassPtr<ULB_RaidPauseMenuWidget> RaidPauseMenuWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "LB|UI|Raid|Pause")
	int32 RaidPauseMenuWidgetZOrder = 100;

	UPROPERTY(Transient)
	TObjectPtr<ULB_RaidPauseMenuWidget> RaidPauseMenuWidget;

	// 입력 모드도 RaidState에 반응해야 하므로 로컬 컨트롤러가 구독 중인 GameState를 보관한다.
	UPROPERTY(Transient)
	TObjectPtr<ALB_RaidGameState> BoundRaidGameState;

	// 이 Controller가 실제로 추가한 컨텍스트만 기록해 다른 시스템이 소유한 매핑을 EndPlay에서 제거하지 않는다.
	UPROPERTY(Transient)
	TSet<TObjectPtr<UInputMappingContext>> AppliedInputMappingContexts;

	FTimerHandle RaidHUDInitRetryTimerHandle;
	TSharedPtr<FStreamableHandle> RaidHUDLoadHandle;
	TSharedPtr<FStreamableHandle> RaidPauseMenuLoadHandle;
	bool bRaidHUDInitializationStopped = false;
	bool bRaidHUDDependencyWaitLogged = false;
	bool bPauseMenuOpen = false;
	bool bPauseMenuOpenPending = false;
	bool bGameplayInputContextsSuspended = false;
	bool bOwnsHostPause = false;
	bool bRaidClientReadyReported = false;

	// 컷씬 종료/중단 시 stale MainMenu ViewTarget 대신 현재 소유 Pawn을 복구하기 위한 로컬 바인딩.
	TWeakObjectPtr<ULevelSequencePlayer> BoundRaidIntroSequencePlayer;
	
	void Jump();
	void StopJumping();
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void PrimaryPressed();
	void SecondaryPressed();
	void PrimaryReleased();
	void SetPrimaryHeld_ServerOnly(bool bHeld);
	void HandlePrimaryRepeat_ServerOnly();
	void StopPrimaryRepeat_ServerOnly();
	bool TryActivatePrimary_ServerOnly();
	float GetSafePrimaryActivationInterval() const;
	bool TryActivateSecondary_ServerOnly();
	float GetSafeSecondaryActivationInterval() const;
	bool IsAlive() const;

	void ApplyInputMappingContexts();
	void RemoveAppliedInputMappingContexts();
	void SuspendGameplayInputContexts();
	void ResumeGameplayInputContexts();
	void ReleaseHeldGameplayInput();
	bool ActivateAbility(const FGameplayTag& AbilityTag) const;
	void LogAbilityActivationFailure(const FGameplayTag& AbilityTag, const UAbilitySystemComponent* ASC) const;
	
	void InitializeRaidHUD();
	void ScheduleRaidHUDInitializationRetry();
	bool AreRaidClientReadyDependenciesReady() const;
	bool AreRaidHUDDependenciesReady() const;
	void RequestRaidHUDClassAsync();
	void HandleRaidHUDClassLoaded();
	void CancelRaidHUDClassLoad();
	void RemoveRaidHUD();
	void InitializePauseMenu();
	void RequestPauseMenuClassAsync();
	void HandlePauseMenuClassLoaded();
	void CancelPauseMenuClassLoad();
	void RemovePauseMenu();
	bool OpenPauseMenu();
	bool IsPauseMenuAllowed() const;
	bool SetOwnedHostPause(bool bShouldPause);
	void RefreshLocalInputPresentation();
	void RefreshPauseOverlay();
	void BindRaidGameState();
	void UnbindRaidGameState();
	void SyncCurrentRaidState();
	void ApplyRaidStatePresentation(ELBRaidState NewState);
	void ReportRaidClientReady();
	void BindRaidIntroSequenceEvents();
	void UnbindRaidIntroSequenceEvents();
	void RestoreGameplayCamera();

	UFUNCTION()
	void HandleRaidIntroEnded();

	UFUNCTION()
	void HandleRaidStateChanged(ELBRaidState NewState);

	UFUNCTION()
	void HandleHostPauseChanged(bool bPaused);
	
	// 눌림/뗌 두 번만 전송하고 연사 주기는 서버 타이머가 담당해 프레임 기반 RPC 폭증을 막는다.
	UFUNCTION(Server, Reliable)
	void ServerSetPrimaryHeld(bool bHeld);

	// 기존 BlueprintCallable 단발 API를 서버 권한으로 전달하는 태그 없는 전용 RPC다.
	UFUNCTION(Server, Reliable)
	void ServerRequestPrimaryAttack();
	
	UFUNCTION(Server, Reliable)
	void ServerActivateSecondary();

	UFUNCTION(Server, Reliable)
	void ServerNotifyRaidClientReady();

	UFUNCTION(Client, Reliable)
	void ClientPlayRaidIntroSequence();
	
};
