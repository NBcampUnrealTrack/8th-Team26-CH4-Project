// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/LB_PlayerController.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Character.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"

#include "GameMode/LB_RaidGameMode.h"
#include "GameState/LB_RaidGameState.h"
#include "GameplayTags/LBTags.h"
#include "Player/LB_PlayerState.h"
#include "TimerManager.h"
#include "Characters/LB_BaseCharacter.h"
#include "UI/HUD/LB_RaidHUDWidget.h"

ALB_PlayerController::ALB_PlayerController()
{
	// 생성자 FClassFinder의 하드 참조를 제거해 서버/비전투 맵 패키지 로드와 메모리 상주를 피한다.
	RaidHUDWidgetClass = TSoftClassPtr<ULB_RaidHUDWidget>(FSoftObjectPath(
		TEXT("/Game/LeftBehind/UI/BattleHUD/HUD/WBP_LB_RaidHUDWidget.WBP_LB_RaidHUDWidget_C")));
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

void ALB_PlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

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
}

void ALB_PlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bRaidHUDInitializationStopped = true;
	bLocalPrimaryHeld = false;
	if (HasAuthority())
	{
		StopPrimaryRepeat_ServerOnly();
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RaidHUDInitRetryTimerHandle);
	}

	CancelRaidHUDClassLoad();
	UnbindRaidGameState();
	RemoveAppliedInputMappingContexts();
	RemoveRaidHUD();

	Super::EndPlay(EndPlayReason);
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
	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	if (!IsAlive()) return;
	
	AddYawInput(LookAxisVector.X);
	AddPitchInput(LookAxisVector.Y);
}

void ALB_PlayerController::PrimaryPressed()
{
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
	if (HasAuthority())
	{
		TryActivatePrimary_ServerOnly();
		return;
	}

	ServerRequestPrimaryAttack();
}

void ALB_PlayerController::ApplyInputMappingContexts()
{
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
		ScheduleRaidHUDInitializationRetry();
		return;
	}

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
	RaidHUDWidget->AddToViewport(RaidHUDWidgetZOrder);
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
			// Pawn/PlayerState/GameState 복제 순서를 Tick으로 감시하지 않고 저비용 one-shot 타이머로 재확인한다.
			TimerManager.SetTimer(
				RaidHUDInitRetryTimerHandle,
				this,
				&ThisClass::InitializeRaidHUD,
				0.1f,
				false);
		}
	}
}

bool ALB_PlayerController::AreRaidHUDDependenciesReady() const
{
	const UWorld* World = GetWorld();
	return IsValid(World)
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
}

void ALB_PlayerController::UnbindRaidGameState()
{
	if (BoundRaidGameState)
	{
		BoundRaidGameState->OnRaidStateChanged.RemoveDynamic(
			this,
			&ThisClass::HandleRaidStateChanged);
		BoundRaidGameState = nullptr;
	}
}

void ALB_PlayerController::SyncCurrentRaidState()
{
	if (IsValid(BoundRaidGameState))
	{
		ApplyRaidStatePresentation(BoundRaidGameState->RaidState);
	}
}

void ALB_PlayerController::HandleRaidStateChanged(ELBRaidState NewState)
{
	ApplyRaidStatePresentation(NewState);
}

void ALB_PlayerController::ApplyRaidStatePresentation(ELBRaidState NewState)
{
	if (GetNetMode() == NM_DedicatedServer || !IsLocalController())
	{
		return;
	}

	if (NewState == ELBRaidState::Result)
	{
		// UIOnly 전환 뒤에는 Completed/Canceled 입력이 오지 않을 수 있으므로 먼저 hold를 해제한다.
		if (bLocalPrimaryHeld)
		{
			PrimaryReleased();
		}
		else if (HasAuthority() && bServerPrimaryHeld)
		{
			StopPrimaryRepeat_ServerOnly();
		}

		bShowMouseCursor = true;
		FInputModeUIOnly InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);

		if (IsValid(RaidHUDWidget))
		{
			// HUD 루트는 입력을 가로채지 않고 자식 버튼만 hit-test를 받게 한다.
			RaidHUDWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
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
