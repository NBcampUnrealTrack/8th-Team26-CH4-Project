// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/LB_PlayerController.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "Components/InputComponent.h"
#include "Components/Widget.h"
#include "Engine/AssetManager.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Character.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Kismet/KismetSystemLibrary.h"

#include "GameMode/LB_RaidGameMode.h"
#include "GameState/LB_RaidGameState.h"
#include "GameplayTags/LBTags.h"
#include "Player/LB_PlayerState.h"
#include "TimerManager.h"
#include "Characters/LB_BaseCharacter.h"
#include "UI/HUD/LB_RaidHUDWidget.h"
#include "UI/Popup/LB_RaidPauseMenuWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogLBRaidUI, Log, All);

ALB_PlayerController::ALB_PlayerController()
{
	// 생성자 FClassFinder의 하드 참조를 제거해 서버/비전투 맵 패키지 로드와 메모리 상주를 피한다.
	RaidHUDWidgetClass = TSoftClassPtr<ULB_RaidHUDWidget>(FSoftObjectPath(
		TEXT("/Game/LeftBehind/UI/BattleHUD/HUD/WBP_LB_RaidHUDWidget.WBP_LB_RaidHUDWidget_C")));
	RaidPauseMenuWidgetClass = TSoftClassPtr<ULB_RaidPauseMenuWidget>(FSoftObjectPath(
		TEXT("/Game/LeftBehind/UI/BattleHUD/Popup/WBP_LB_RaidPauseMenuWidget.WBP_LB_RaidPauseMenuWidget_C")));
}

bool ALB_PlayerController::IsLocalListenHost() const
{
	const ENetMode NetMode = GetNetMode();
	return IsLocalController()
		&& HasAuthority()
		&& (NetMode == NM_ListenServer || NetMode == NM_Standalone);
}

bool ALB_PlayerController::CanRequestReturnToMainMenu() const
{
	if (!IsLocalListenHost())
	{
		return false;
	}

	ALB_RaidGameMode* RaidGameMode = GetWorld()
		? GetWorld()->GetAuthGameMode<ALB_RaidGameMode>()
		: nullptr;
	return IsValid(RaidGameMode)
		&& RaidGameMode->CanReturnToMainMenu(this);
}

bool ALB_PlayerController::RequestReturnToMainMenu()
{
	if (!IsLocalListenHost())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[LB Raid] Remote return-to-menu request rejected. Controller=%s"),
			*GetNameSafe(this));
		return false;
	}

	ALB_RaidGameMode* RaidGameMode = GetWorld()
		? GetWorld()->GetAuthGameMode<ALB_RaidGameMode>()
		: nullptr;
	return IsValid(RaidGameMode) && RaidGameMode->TryReturnToMainMenu(this);
}

bool ALB_PlayerController::CanRequestAbortRaidToRoom() const
{
	if (!IsLocalListenHost())
	{
		return false;
	}

	const ALB_RaidGameMode* RaidGameMode = GetWorld()
		? GetWorld()->GetAuthGameMode<ALB_RaidGameMode>()
		: nullptr;
	return IsValid(RaidGameMode) && RaidGameMode->CanAbortRaidToRoom(this);
}

bool ALB_PlayerController::RequestAbortRaidToRoom()
{
	if (!CanRequestAbortRaidToRoom())
	{
		return false;
	}

	ALB_RaidGameMode* RaidGameMode = GetWorld()
		? GetWorld()->GetAuthGameMode<ALB_RaidGameMode>()
		: nullptr;
	if (!IsValid(RaidGameMode) || !RaidGameMode->TryAbortRaidToRoom(this))
	{
		// GameMode가 즉시 이동 실패 시 전역 Pause를 원래 상태로 복원한다.
		RefreshPauseOverlay();
		return false;
	}

	// ServerTravel이 시작되었으므로 현재 월드의 입력을 다시 켜지 않는다.
	bOwnsHostPause = false;
	bPauseMenuOpen = false;
	bPauseMenuOpenPending = false;
	if (IsValid(RaidPauseMenuWidget))
	{
		RaidPauseMenuWidget->HidePauseOverlay();
	}
	return true;
}

void ALB_PlayerController::TogglePauseMenu()
{
	// 첫 ESC가 비동기 로드 완료 후 열기를 예약했다면 두 번째 ESC는 그 예약을 취소한다.
	// 클래스 프리로드 자체는 유지해 다음 요청을 즉시 처리할 수 있게 한다.
	if (!bPauseMenuOpen && bPauseMenuOpenPending)
	{
		bPauseMenuOpenPending = false;
		return;
	}

	if (bPauseMenuOpen)
	{
		if (IsValid(RaidPauseMenuWidget) && RaidPauseMenuWidget->IsShowingQuitConfirmation())
		{
			RaidPauseMenuWidget->CancelQuitConfirmation();
			RefreshLocalInputPresentation();
			return;
		}

		ClosePauseMenu();
		return;
	}

	OpenPauseMenu();
}

void ALB_PlayerController::ClosePauseMenu()
{
	bPauseMenuOpenPending = false;
	if (!bPauseMenuOpen)
	{
		RefreshPauseOverlay();
		return;
	}

	// 호스트가 소유한 전역 Pause를 해제하지 못했다면 조작 불능 화면이 되지 않도록 메뉴를 유지한다.
	if (bOwnsHostPause && !SetOwnedHostPause(false))
	{
		UE_LOG(LogLBRaidUI, Error, TEXT("Failed to release the pause-menu owned host pause. Controller=%s"), *GetNameSafe(this));
		return;
	}

	bPauseMenuOpen = false;
	ResumeGameplayInputContexts();
	RefreshLocalInputPresentation();
	RefreshPauseOverlay();
}

void ALB_PlayerController::ConfirmQuitGame()
{
	if (!IsLocalController())
	{
		return;
	}

	// 종료 직전에도 서버를 정지 상태로 남기지 않도록 최선의 노력으로 소유 Pause를 해제한다.
	SetOwnedHostPause(false);
	bPauseMenuOpen = false;
	bPauseMenuOpenPending = false;
	if (IsValid(RaidPauseMenuWidget))
	{
		RaidPauseMenuWidget->HidePauseOverlay();
	}

	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

void ALB_PlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (IsValid(InputComponent))
	{
		// 정지 키 설정
		FInputKeyBinding& PauseBinding = InputComponent->BindKey(
			EKeys::Zero,
			IE_Pressed,
			this,
			&ThisClass::TogglePauseMenu);
		PauseBinding.bExecuteWhenPaused = true;
		PauseBinding.bConsumeInput = true;
	}

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (!IsValid(EnhancedInputComponent)) return;

	ApplyInputMappingContexts();

	if (IsValid(JumpAction))
	{
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ThisClass::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ThisClass::StopJumping);
	}
	if (IsValid(MoveAction))
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ThisClass::Move);
	}
	if (IsValid(LookAction))
	{
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ThisClass::Look);
	}
	if (IsValid(PrimaryAction))
	{
		// 입력 프레임마다 RPC를 보내지 않고 눌림/뗌 상태 전환만 서버로 보낸다.
		EnhancedInputComponent->BindAction(PrimaryAction, ETriggerEvent::Started, this, &ThisClass::PrimaryPressed);
		EnhancedInputComponent->BindAction(PrimaryAction, ETriggerEvent::Completed, this, &ThisClass::PrimaryReleased);
		EnhancedInputComponent->BindAction(PrimaryAction, ETriggerEvent::Canceled, this, &ThisClass::PrimaryReleased);
	}
}

void ALB_PlayerController::BeginPlay()
{
	Super::BeginPlay();
	bRaidHUDInitializationStopped = false;

	bShowMouseCursor = false;

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);

	ApplyInputMappingContexts();
	InitializeRaidHUD();
	InitializePauseMenu();
}

void ALB_PlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bRaidHUDInitializationStopped = true;
	bPauseMenuOpenPending = false;
	ReleaseHeldGameplayInput();

	// seamless travel/종료 전에 이 메뉴가 건 전역 Pause만 해제한다.
	if (bOwnsHostPause && !SetOwnedHostPause(false) && HasAuthority() && IsPaused())
	{
		SetPause(false);
		bOwnsHostPause = false;
	}
	bPauseMenuOpen = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RaidHUDInitRetryTimerHandle);
	}

	CancelRaidHUDClassLoad();
	CancelPauseMenuClassLoad();
	UnbindRaidGameState();
	RemoveAppliedInputMappingContexts();
	RemoveRaidHUD();
	RemovePauseMenu();

	Super::EndPlay(EndPlayReason);
}

void ALB_PlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();

	// Seamless travel 중 새 PlayerController의 BeginPlay가 ULocalPlayer 연결보다 먼저 올 수 있다.
	// ReceivedPlayer는 owning client가 확정된 시점이므로 여기서 HUD 초기화를 반드시 재진입한다.
	ApplyInputMappingContexts();
	InitializeRaidHUD();
	InitializePauseMenu();
}

void ALB_PlayerController::BeginPlayingState()
{
	Super::BeginPlayingState();

	// Pawn 전환이 끝난 시점에도 한 번 더 확인한다. InitializeRaidHUD는 중복 생성에 안전하다.
	InitializeRaidHUD();
	InitializePauseMenu();
}

void ALB_PlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// 원격 클라이언트의 PlayerState 복제가 늦게 도착하면 이벤트 기반으로 즉시 재확인한다.
	InitializeRaidHUD();
	InitializePauseMenu();
}

void ALB_PlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);

	ApplyInputMappingContexts();

	// Pawn/PlayerState/ASC가 늦게 준비될 수 있으므로 Possess 이후에도 HUD 생성을 보장한다.
	InitializeRaidHUD();
}


void ALB_PlayerController::OnUnPossess()
{
	// 서버 타이머가 이전 Pawn의 ASC를 계속 발동하지 않도록 소유 해제 시 즉시 연사를 중단한다.
	if (HasAuthority())
	{
		StopPrimaryRepeat_ServerOnly();
	}
	bLocalPrimaryHeld = false;

	Super::OnUnPossess();
}

void ALB_PlayerController::Jump()
{
	if (bPauseMenuOpen) return;

	// GetCharacter 내부 Cast/조회 결과를 한 번만 사용해 입력 핫패스의 중복 작업을 없앤다.
	ACharacter* ControlledCharacter = GetCharacter();
	if (!IsValid(ControlledCharacter)) return;
	if (!IsAlive()) return;

	ControlledCharacter->Jump();
}

void ALB_PlayerController::StopJumping()
{
	ACharacter* ControlledCharacter = GetCharacter();
	if (!IsValid(ControlledCharacter)) return;

	ControlledCharacter->StopJumping();
}

void ALB_PlayerController::Move(const FInputActionValue& Value)
{
	if (bPauseMenuOpen) return;

	APawn* ControlledPawn = GetPawn();
	if (!IsValid(ControlledPawn)) return;
	if (!IsAlive()) return;
	
	const FVector2D MovementVector = Value.Get<FVector2D>();
	
	const FRotator YawRotation(0.f, GetControlRotation().Yaw, 0.f);
	// 동일 회전 행렬을 한 번만 만들어 전/우 방향 벡터에 재사용한다.
	const FRotationMatrix YawRotationMatrix(YawRotation);
	const FVector ForwardDirection = YawRotationMatrix.GetUnitAxis(EAxis::X);
	const FVector RightDirection = YawRotationMatrix.GetUnitAxis(EAxis::Y);
	
	ControlledPawn->AddMovementInput(ForwardDirection, MovementVector.Y);
	ControlledPawn->AddMovementInput(RightDirection, MovementVector.X);
}

void ALB_PlayerController::Look(const FInputActionValue& Value)
{
	if (bPauseMenuOpen) return;

	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	if (!IsAlive()) return;
	
	AddYawInput(LookAxisVector.X);
	AddPitchInput(LookAxisVector.Y);
}

void ALB_PlayerController::PrimaryPressed()
{
	if (bPauseMenuOpen) return;

	if (!IsAlive()) return;
	if (!IsLocalController() || bLocalPrimaryHeld)
	{
		return;
	}

	bLocalPrimaryHeld = true;
	if (HasAuthority())
	{
		SetPrimaryHeld_ServerOnly(true);
		return;
	}

	ServerSetPrimaryHeld(true);
}

void ALB_PlayerController::PrimaryReleased()
{
	// Completed와 Canceled가 같은 프레임에 들어와도 release RPC는 한 번만 전송한다.
	if (!bLocalPrimaryHeld)
	{
		return;
	}

	bLocalPrimaryHeld = false;
	if (HasAuthority())
	{
		SetPrimaryHeld_ServerOnly(false);
		return;
	}

	ServerSetPrimaryHeld(false);
}

void ALB_PlayerController::SetPrimaryHeld_ServerOnly(bool bHeld)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!bHeld)
	{
		if (bServerPrimaryHeld)
		{
			StopPrimaryRepeat_ServerOnly();
		}
		return;
	}

	if (bServerPrimaryHeld || !IsValid(GetPawn()))
	{
		return;
	}

	bServerPrimaryHeld = true;
	TryActivatePrimary_ServerOnly();

	if (UWorld* World = GetWorld())
	{
		// 반복 공격은 서버의 단일 타이머가 담당해 클라이언트 FPS와 네트워크 지연에 영향을 받지 않는다.
		const float SafeInterval = GetSafePrimaryActivationInterval();
		World->GetTimerManager().SetTimer(
			ServerPrimaryRepeatTimerHandle,
			this,
			&ThisClass::HandlePrimaryRepeat_ServerOnly,
			SafeInterval,
			true,
			SafeInterval);
	}
	else
	{
		bServerPrimaryHeld = false;
	}
}

void ALB_PlayerController::HandlePrimaryRepeat_ServerOnly()
{
	if (!HasAuthority() || !bServerPrimaryHeld || !IsValid(GetPawn()))
	{
		StopPrimaryRepeat_ServerOnly();
		return;
	}

	TryActivatePrimary_ServerOnly();
}

void ALB_PlayerController::StopPrimaryRepeat_ServerOnly()
{
	bServerPrimaryHeld = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ServerPrimaryRepeatTimerHandle);
	}
}

bool ALB_PlayerController::TryActivatePrimary_ServerOnly()
{
	if (!HasAuthority())
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	const double CurrentTime = World->GetTimeSeconds();
	const double SafeInterval = static_cast<double>(GetSafePrimaryActivationInterval());
	if (LastPrimaryActivationServerTime >= 0.0
		&& CurrentTime >= LastPrimaryActivationServerTime
		&& CurrentTime - LastPrimaryActivationServerTime + UE_KINDA_SMALL_NUMBER < SafeInterval)
	{
		return false;
	}

	// 성공 여부와 무관하게 시도 시각을 기록해 실패 상태에서 Reliable RPC를 연속 호출하는 남용도 제한한다.
	LastPrimaryActivationServerTime = CurrentTime;
	return ActivateAbility(LBTags::LBAbilities::Primary);
}

float ALB_PlayerController::GetSafePrimaryActivationInterval() const
{
	return FMath::IsFinite(PrimaryActivationInterval) && PrimaryActivationInterval >= 0.01f
		? PrimaryActivationInterval
		: 0.3f;
}

bool ALB_PlayerController::IsAlive() const
{
	ALB_BaseCharacter* BaseCharacter = Cast<ALB_BaseCharacter>(GetPawn());
	if (!IsValid(BaseCharacter)) return false;
	return BaseCharacter->IsAlive();
}

void ALB_PlayerController::RequestPrimaryAttack()
{
	if (bPauseMenuOpen)
	{
		return;
	}

	if (HasAuthority())
	{
		TryActivatePrimary_ServerOnly();
		return;
	}

	ServerRequestPrimaryAttack();
}

void ALB_PlayerController::ApplyInputMappingContexts()
{
	if (bGameplayInputContextsSuspended)
	{
		return;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!IsValid(LocalPlayer)) return;

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);
	if (!IsValid(InputSubsystem)) return;

	for (UInputMappingContext* Context : InputMappingContexts)
	{
		if (IsValid(Context) && !InputSubsystem->HasMappingContext(Context))
		{
			// Setup/Begin/Possess 재호출에도 중복 매핑 재빌드를 하지 않고, 직접 추가한 항목만 소유로 기록한다.
			InputSubsystem->AddMappingContext(Context, 0);
			AppliedInputMappingContexts.Add(Context);
		}
	}
}

void ALB_PlayerController::RemoveAppliedInputMappingContexts()
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = IsValid(LocalPlayer)
		? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer)
		: nullptr;

	if (IsValid(InputSubsystem))
	{
		for (UInputMappingContext* Context : AppliedInputMappingContexts)
		{
			if (IsValid(Context) && InputSubsystem->HasMappingContext(Context))
			{
				InputSubsystem->RemoveMappingContext(Context);
			}
		}
	}

	AppliedInputMappingContexts.Empty();
}

void ALB_PlayerController::SuspendGameplayInputContexts()
{
	if (bGameplayInputContextsSuspended)
	{
		return;
	}

	bGameplayInputContextsSuspended = true;
	RemoveAppliedInputMappingContexts();
}

void ALB_PlayerController::ResumeGameplayInputContexts()
{
	if (!bGameplayInputContextsSuspended)
	{
		return;
	}

	bGameplayInputContextsSuspended = false;
	ApplyInputMappingContexts();
}

void ALB_PlayerController::ReleaseHeldGameplayInput()
{
	if (ACharacter* ControlledCharacter = GetCharacter())
	{
		ControlledCharacter->StopJumping();
	}

	if (bLocalPrimaryHeld)
	{
		PrimaryReleased();
	}
	else if (HasAuthority() && bServerPrimaryHeld)
	{
		StopPrimaryRepeat_ServerOnly();
	}
}

bool ALB_PlayerController::ActivateAbility(const FGameplayTag& AbilityTag) const
{
	// PlayerState가 ASC의 실제 소유자이므로 타입이 보장된 경로를 먼저 사용해 반복 reflection/Cast 비용을 줄인다.
	const ALB_PlayerState* LBPlayerState = GetPlayerState<ALB_PlayerState>();
	UAbilitySystemComponent* ASC = IsValid(LBPlayerState)
		? LBPlayerState->GetAbilitySystemComponent()
		: nullptr;
	if (!IsValid(ASC))
	{
		// Pawn의 PlayerState 연결이 먼저 완료된 예외 순서를 위해 기존 GAS 인터페이스 fallback은 유지한다.
		ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetPawn());
	}

	if (!IsValid(ASC))
	{
		LogAbilityActivationFailure(AbilityTag, nullptr);
		return false;
	}

	// 서버가 ASC에서 능력을 켠다. 그래서 클라이언트가 임의로 보스 HP를 바꿀 수 없다.
	const bool bActivated = ASC->TryActivateAbilitiesByTag(AbilityTag.GetSingleTagContainer());
	if (!bActivated)
	{
		LogAbilityActivationFailure(AbilityTag, ASC);
	}

	return bActivated;
}

void ALB_PlayerController::LogAbilityActivationFailure(const FGameplayTag& AbilityTag, const UAbilitySystemComponent* ASC) const
{
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[LB Input] Ability activation failed. Controller=%s Pawn=%s PlayerState=%s ASC=%s Tag=%s Authority=%d Local=%d"),
		*GetNameSafe(this),
		*GetNameSafe(GetPawn()),
		*GetNameSafe(PlayerState.Get()),
		*GetNameSafe(ASC),
		*AbilityTag.ToString(),
		HasAuthority() ? 1 : 0,
		IsLocalController() ? 1 : 0
	);
}

void ALB_PlayerController::ServerSetPrimaryHeld_Implementation(bool bHeld)
{
	SetPrimaryHeld_ServerOnly(bHeld);
}

void ALB_PlayerController::ServerRequestPrimaryAttack_Implementation()
{
	TryActivatePrimary_ServerOnly();
}

void ALB_PlayerController::InitializeRaidHUD()
{
	// Dedicated Server에는 Slate와 HUD 애셋이 필요 없으므로 로드 요청 자체를 만들지 않는다.
	if (bRaidHUDInitializationStopped || GetNetMode() == NM_DedicatedServer || !IsLocalController())
	{
		return;
	}

	// UI 클래스 로드가 늦어져도 현재 Result 상태의 입력 모드는 즉시 적용한다.
	BindRaidGameState();
	SyncCurrentRaidState();

	if (IsValid(RaidHUDWidget))
	{
		return;
	}

	if (!AreRaidHUDDependenciesReady())
	{
		if (!bRaidHUDDependencyWaitLogged)
		{
			const UWorld* World = GetWorld();
			UE_LOG(
				LogLBRaidUI,
				Log,
				TEXT("Raid HUD is waiting for client dependencies. Controller=%s LocalPlayer=%s Pawn=%s PlayerState=%s GameState=%s"),
				*GetNameSafe(this),
				*GetNameSafe(GetLocalPlayer()),
				*GetNameSafe(GetPawn()),
				*GetNameSafe(GetPlayerState<ALB_PlayerState>()),
				*GetNameSafe(World ? World->GetGameState<ALB_RaidGameState>() : nullptr));
			bRaidHUDDependencyWaitLogged = true;
		}
		ScheduleRaidHUDInitializationRetry();
		return;
	}
	bRaidHUDDependencyWaitLogged = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RaidHUDInitRetryTimerHandle);
	}

	if (RaidHUDWidgetClass.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LB UI] RaidHUDWidgetClass is not set. Controller=%s"), *GetNameSafe(this));
		return;
	}

	UClass* LoadedHUDWidgetClass = RaidHUDWidgetClass.Get();
	if (!IsValid(LoadedHUDWidgetClass))
	{
		RequestRaidHUDClassAsync();
		return;
	}

	RaidHUDWidget = CreateWidget<ULB_RaidHUDWidget>(this, LoadedHUDWidgetClass);
	if (!IsValid(RaidHUDWidget))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LB UI] Failed to create RaidHUDWidget. Controller=%s"), *GetNameSafe(this));
		return;
	}

	// UI는 각 로컬 PlayerController가 자기 화면에 붙인다.
	// GameMode는 서버에만 있으므로 클라이언트 화면 UI를 만들면 안 된다.
	if (!RaidHUDWidget->AddToPlayerScreen(RaidHUDWidgetZOrder))
	{
		UE_LOG(
			LogLBRaidUI,
			Warning,
			TEXT("Failed to attach Raid HUD to the local player's screen. Controller=%s"),
			*GetNameSafe(this));
		RaidHUDWidget = nullptr;
		ScheduleRaidHUDInitializationRetry();
		return;
	}

	UE_LOG(
		LogLBRaidUI,
		Log,
		TEXT("Raid HUD attached to local player screen. Controller=%s Widget=%s"),
		*GetNameSafe(this),
		*GetNameSafe(RaidHUDWidget));
	// 생성 시점의 현재 상태를 다시 적용해 Result 도중 로드된 HUD도 버튼 hit-test가 가능하게 한다.
	SyncCurrentRaidState();
}

void ALB_PlayerController::ScheduleRaidHUDInitializationRetry()
{
	if (bRaidHUDInitializationStopped)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		if (!TimerManager.IsTimerActive(RaidHUDInitRetryTimerHandle))
		{
			// 원격 클라이언트의 Pawn/PlayerState/GameState는 여러 네트워크 프레임에 걸쳐 도착할 수 있다.
			// 성공 경로와 EndPlay에서 명시적으로 해제하는 저주기 타이머로 준비될 때까지 재확인한다.
			TimerManager.SetTimer(
				RaidHUDInitRetryTimerHandle,
				this,
				&ThisClass::InitializeRaidHUD,
				0.1f,
				true);
		}
	}
}

bool ALB_PlayerController::AreRaidHUDDependenciesReady() const
{
	const UWorld* World = GetWorld();
	return IsValid(World)
		&& IsValid(GetLocalPlayer())
		&& IsValid(GetPawn())
		&& IsValid(GetPlayerState<ALB_PlayerState>())
		&& IsValid(World->GetGameState<ALB_RaidGameState>());
}

void ALB_PlayerController::RequestRaidHUDClassAsync()
{
	if (bRaidHUDInitializationStopped || RaidHUDLoadHandle.IsValid())
	{
		return;
	}

	const FSoftObjectPath HUDClassPath = RaidHUDWidgetClass.ToSoftObjectPath();
	if (!HUDClassPath.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LB UI] Raid HUD soft class path is invalid. Controller=%s"), *GetNameSafe(this));
		return;
	}

	// HUD 패키지를 로컬 플레이어에게만 비동기 스트리밍해 게임 스레드 hitch와 서버 메모리 상주를 방지한다.
	RaidHUDLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		HUDClassPath,
		FStreamableDelegate::CreateUObject(this, &ThisClass::HandleRaidHUDClassLoaded),
		FStreamableManager::DefaultAsyncLoadPriority,
		false,
		false,
		TEXT("LB_RaidHUDWidget"));

	if (!RaidHUDLoadHandle.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LB UI] Failed to request Raid HUD async load. Path=%s Controller=%s"),
			*HUDClassPath.ToString(),
			*GetNameSafe(this));
	}
}

void ALB_PlayerController::HandleRaidHUDClassLoaded()
{
	RaidHUDLoadHandle.Reset();
	if (bRaidHUDInitializationStopped)
	{
		return;
	}

	if (!IsValid(RaidHUDWidgetClass.Get()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LB UI] Raid HUD async load completed without a valid class. Path=%s Controller=%s"),
			*RaidHUDWidgetClass.ToSoftObjectPath().ToString(),
			*GetNameSafe(this));
		return;
	}

	InitializeRaidHUD();
}

void ALB_PlayerController::CancelRaidHUDClassLoad()
{
	if (!RaidHUDLoadHandle.IsValid())
	{
		return;
	}

	if (!RaidHUDLoadHandle->HasLoadCompleted())
	{
		RaidHUDLoadHandle->CancelHandle();
	}
	RaidHUDLoadHandle.Reset();
}

void ALB_PlayerController::RemoveRaidHUD()
{
	if (IsValid(RaidHUDWidget))
	{
		RaidHUDWidget->RemoveFromParent();
		RaidHUDWidget = nullptr;
	}
}

void ALB_PlayerController::InitializePauseMenu()
{
	if (bRaidHUDInitializationStopped || GetNetMode() == NM_DedicatedServer || !IsLocalController())
	{
		return;
	}

	BindRaidGameState();
	if (IsValid(RaidPauseMenuWidget))
	{
		RefreshPauseOverlay();
		return;
	}

	if (RaidPauseMenuWidgetClass.IsNull())
	{
		UE_LOG(LogLBRaidUI, Warning, TEXT("RaidPauseMenuWidgetClass is not set. Controller=%s"), *GetNameSafe(this));
		bPauseMenuOpenPending = false;
		return;
	}

	UClass* LoadedPauseMenuClass = RaidPauseMenuWidgetClass.Get();
	if (!IsValid(LoadedPauseMenuClass))
	{
		RequestPauseMenuClassAsync();
		return;
	}

	RaidPauseMenuWidget = CreateWidget<ULB_RaidPauseMenuWidget>(this, LoadedPauseMenuClass);
	if (!IsValid(RaidPauseMenuWidget))
	{
		UE_LOG(LogLBRaidUI, Warning, TEXT("Failed to create raid pause menu. Controller=%s"), *GetNameSafe(this));
		bPauseMenuOpenPending = false;
		return;
	}

	if (!RaidPauseMenuWidget->AddToPlayerScreen(RaidPauseMenuWidgetZOrder))
	{
		UE_LOG(LogLBRaidUI, Warning, TEXT("Failed to attach raid pause menu. Controller=%s"), *GetNameSafe(this));
		RaidPauseMenuWidget = nullptr;
		bPauseMenuOpenPending = false;
		return;
	}

	RaidPauseMenuWidget->HidePauseOverlay();
	const bool bShouldOpenWhenReady = bPauseMenuOpenPending;
	bPauseMenuOpenPending = false;
	RefreshPauseOverlay();
	if (bShouldOpenWhenReady)
	{
		OpenPauseMenu();
	}
}

void ALB_PlayerController::RequestPauseMenuClassAsync()
{
	if (bRaidHUDInitializationStopped || RaidPauseMenuLoadHandle.IsValid())
	{
		return;
	}

	const FSoftObjectPath PauseMenuClassPath = RaidPauseMenuWidgetClass.ToSoftObjectPath();
	if (!PauseMenuClassPath.IsValid())
	{
		UE_LOG(LogLBRaidUI, Error, TEXT("Raid pause menu class path is invalid. Path=%s"), *PauseMenuClassPath.ToString());
		bPauseMenuOpenPending = false;
		return;
	}

	RaidPauseMenuLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		PauseMenuClassPath,
		FStreamableDelegate::CreateUObject(this, &ThisClass::HandlePauseMenuClassLoaded),
		FStreamableManager::DefaultAsyncLoadPriority,
		false,
		false,
		TEXT("LB_RaidPauseMenuWidget"));

	if (!RaidPauseMenuLoadHandle.IsValid())
	{
		UE_LOG(LogLBRaidUI, Error, TEXT("Failed to request raid pause menu load. Path=%s"), *PauseMenuClassPath.ToString());
		bPauseMenuOpenPending = false;
	}
}

void ALB_PlayerController::HandlePauseMenuClassLoaded()
{
	RaidPauseMenuLoadHandle.Reset();
	if (bRaidHUDInitializationStopped)
	{
		return;
	}

	if (!IsValid(RaidPauseMenuWidgetClass.Get()))
	{
		UE_LOG(LogLBRaidUI, Error, TEXT("Raid pause menu async load completed without a valid class. Path=%s"),
			*RaidPauseMenuWidgetClass.ToSoftObjectPath().ToString());
		bPauseMenuOpenPending = false;
		return;
	}

	InitializePauseMenu();
}

void ALB_PlayerController::CancelPauseMenuClassLoad()
{
	if (!RaidPauseMenuLoadHandle.IsValid())
	{
		return;
	}

	if (!RaidPauseMenuLoadHandle->HasLoadCompleted())
	{
		RaidPauseMenuLoadHandle->CancelHandle();
	}
	RaidPauseMenuLoadHandle.Reset();
}

void ALB_PlayerController::RemovePauseMenu()
{
	if (IsValid(RaidPauseMenuWidget))
	{
		RaidPauseMenuWidget->RemoveFromParent();
		RaidPauseMenuWidget = nullptr;
	}
}

bool ALB_PlayerController::IsPauseMenuAllowed() const
{
	if (GetNetMode() == NM_DedicatedServer || !IsLocalController() || !IsValid(BoundRaidGameState))
	{
		return false;
	}

	switch (BoundRaidGameState->RaidState)
	{
	case ELBRaidState::Waiting:
	case ELBRaidState::Countdown:
	case ELBRaidState::Battle:
		return true;
	case ELBRaidState::Result:
	default:
		return false;
	}
}

bool ALB_PlayerController::SetOwnedHostPause(bool bShouldPause)
{
	if (bShouldPause)
	{
		if (bOwnsHostPause)
		{
			return true;
		}
		if (!IsLocalListenHost())
		{
			return false;
		}
	}
	else if (!bOwnsHostPause)
	{
		return true;
	}

	ALB_RaidGameMode* RaidGameMode = GetWorld()
		? GetWorld()->GetAuthGameMode<ALB_RaidGameMode>()
		: nullptr;
	bool bSucceeded = IsValid(RaidGameMode)
		&& RaidGameMode->TrySetHostPause(this, bShouldPause);

	if (!bShouldPause && !bSucceeded && !IsPaused())
	{
		// 이동/종료 과정에서 GameMode가 먼저 Pause를 정리한 경우도 성공으로 취급한다.
		bSucceeded = true;
	}

	if (bSucceeded)
	{
		bOwnsHostPause = bShouldPause;
	}
	return bSucceeded;
}

bool ALB_PlayerController::OpenPauseMenu()
{
	if (bPauseMenuOpen || !IsPauseMenuAllowed())
	{
		bPauseMenuOpenPending = false;
		return false;
	}

	if (!IsValid(RaidPauseMenuWidget))
	{
		bPauseMenuOpenPending = true;
		InitializePauseMenu();
		return false;
	}

	ReleaseHeldGameplayInput();
	if (IsLocalListenHost() && !SetOwnedHostPause(true))
	{
		UE_LOG(LogLBRaidUI, Warning, TEXT("Host pause-menu open rejected by GameMode. Controller=%s"), *GetNameSafe(this));
		return false;
	}

	bPauseMenuOpenPending = false;
	bPauseMenuOpen = true;
	SuspendGameplayInputContexts();
	RaidPauseMenuWidget->ShowActionMenu(CanRequestAbortRaidToRoom());
	RefreshLocalInputPresentation();
	return true;
}

void ALB_PlayerController::RefreshLocalInputPresentation()
{
	if (GetNetMode() == NM_DedicatedServer || !IsLocalController())
	{
		return;
	}

	const ELBRaidState CurrentState = IsValid(BoundRaidGameState)
		? BoundRaidGameState->RaidState
		: ELBRaidState::Waiting;
	if (CurrentState == ELBRaidState::Result)
	{
		bShowMouseCursor = true;
		FInputModeUIOnly InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);

		if (IsValid(RaidHUDWidget))
		{
			RaidHUDWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		return;
	}

	if (bPauseMenuOpen && IsValid(RaidPauseMenuWidget))
	{
		bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		if (UWidget* FocusTarget = RaidPauseMenuWidget->GetPreferredFocusTarget())
		{
			InputMode.SetWidgetToFocus(FocusTarget->TakeWidget());
		}
		SetInputMode(InputMode);

		if (IsValid(RaidHUDWidget))
		{
			RaidHUDWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		return;
	}

	bShowMouseCursor = false;
	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);

	if (IsValid(RaidHUDWidget))
	{
		RaidHUDWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void ALB_PlayerController::RefreshPauseOverlay()
{
	if (!IsValid(RaidPauseMenuWidget))
	{
		return;
	}

	if (bPauseMenuOpen)
	{
		RaidPauseMenuWidget->SetCanReturnToRoom(CanRequestAbortRaidToRoom());
		return;
	}

	const bool bResultState = IsValid(BoundRaidGameState)
		&& BoundRaidGameState->RaidState == ELBRaidState::Result;
	const bool bShowRemoteHostNotice = !bResultState
		&& IsValid(BoundRaidGameState)
		&& BoundRaidGameState->bHostPauseActive
		&& !IsLocalListenHost();
	if (bShowRemoteHostNotice)
	{
		RaidPauseMenuWidget->ShowHostPauseNotice();
	}
	else
	{
		RaidPauseMenuWidget->HidePauseOverlay();
	}
}

void ALB_PlayerController::BindRaidGameState()
{
	ALB_RaidGameState* CurrentRaidGameState = GetWorld()
		? GetWorld()->GetGameState<ALB_RaidGameState>()
		: nullptr;
	if (BoundRaidGameState == CurrentRaidGameState)
	{
		return;
	}

	UnbindRaidGameState();
	if (!IsValid(CurrentRaidGameState))
	{
		return;
	}

	BoundRaidGameState = CurrentRaidGameState;
	BoundRaidGameState->OnRaidStateChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleRaidStateChanged);
	BoundRaidGameState->OnHostPauseChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleHostPauseChanged);
}

void ALB_PlayerController::UnbindRaidGameState()
{
	if (BoundRaidGameState)
	{
		BoundRaidGameState->OnRaidStateChanged.RemoveDynamic(
			this,
			&ThisClass::HandleRaidStateChanged);
		BoundRaidGameState->OnHostPauseChanged.RemoveDynamic(
			this,
			&ThisClass::HandleHostPauseChanged);
		BoundRaidGameState = nullptr;
	}
}

void ALB_PlayerController::SyncCurrentRaidState()
{
	if (IsValid(BoundRaidGameState))
	{
		ApplyRaidStatePresentation(BoundRaidGameState->RaidState);
		return;
	}

	RefreshLocalInputPresentation();
	RefreshPauseOverlay();
}

void ALB_PlayerController::HandleRaidStateChanged(ELBRaidState NewState)
{
	ApplyRaidStatePresentation(NewState);
}

void ALB_PlayerController::HandleHostPauseChanged(bool bPaused)
{
	(void)bPaused;
	RefreshPauseOverlay();
}

void ALB_PlayerController::ApplyRaidStatePresentation(ELBRaidState NewState)
{
	if (GetNetMode() == NM_DedicatedServer || !IsLocalController())
	{
		return;
	}

	if (NewState == ELBRaidState::Result)
	{
		// Result UI가 항상 최우선이다. 메뉴가 소유한 Pause와 입력 차단을 먼저 정리한다.
		ReleaseHeldGameplayInput();
		if (bPauseMenuOpen)
		{
			ClosePauseMenu();
		}
		bPauseMenuOpenPending = false;
	}

	RefreshLocalInputPresentation();
	RefreshPauseOverlay();
}
