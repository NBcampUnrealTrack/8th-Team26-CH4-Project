// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/LB_PlayerController.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"

#include "GameplayTags/LBTags.h"
#include "TimerManager.h"

#include "UI/HUD/LB_RaidHUDWidget.h"

#include "UObject/ConstructorHelpers.h"

ALB_PlayerController::ALB_PlayerController()
{
	static ConstructorHelpers::FClassFinder<ULB_RaidHUDWidget> RaidHUDClassFinder(
		TEXT("/Game/LeftBehind/UI/BattleHUD/HUD/WBP_LB_RaidHUDWidget"));

	if (RaidHUDClassFinder.Succeeded())
	{
		RaidHUDWidgetClass = RaidHUDClassFinder.Class;
	}
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
		EnhancedInputComponent->BindAction(PrimaryAction, ETriggerEvent::Triggered, this, &ThisClass::Primary);
	}
}

void ALB_PlayerController::BeginPlay()
{
	Super::BeginPlay();

	bShowMouseCursor = false;

	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);

	ApplyInputMappingContexts();
	InitializeRaidHUD();

	if (HasAuthority())
	{
		// Listen Server에서 원격 클라이언트도 자기 화면에 HUD를 만들게 한다.
		ClientInitializeRaidHUD();
	}
}

void ALB_PlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RaidHUDInitRetryTimerHandle);
	}

	RemoveRaidHUD();

	Super::EndPlay(EndPlayReason);
}

void ALB_PlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);

	ApplyInputMappingContexts();

	// Pawn/PlayerState/ASC가 늦게 준비될 수 있으므로 Possess 이후에도 HUD 생성을 보장한다.
	InitializeRaidHUD();

	if (HasAuthority())
	{
		ClientInitializeRaidHUD();
	}
}

void ALB_PlayerController::Jump()
{
	if (!IsValid(GetCharacter())) return;
	
	GetCharacter()->Jump();
}

void ALB_PlayerController::StopJumping()
{
	if (!IsValid(GetCharacter())) return;
	GetCharacter()->StopJumping();
}

void ALB_PlayerController::Move(const FInputActionValue& Value)
{
	if (!IsValid(GetPawn())) return;
	
	const FVector2D MovementVector = Value.Get<FVector2D>();
	
	const FRotator YawRotation(0.f, GetControlRotation().Yaw, 0.f);
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	
	GetPawn()->AddMovementInput(ForwardDirection, MovementVector.Y);
	GetPawn()->AddMovementInput(RightDirection, MovementVector.X);
}

void ALB_PlayerController::Look(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	
	AddYawInput(LookAxisVector.X);
	AddPitchInput(LookAxisVector.Y);
}

void ALB_PlayerController::Primary()
{
	const UWorld* World = GetWorld();
	const float CurrentTime = World ? World->GetTimeSeconds() : 0.f;
	if (CurrentTime - LastPrimaryActivationTime < PrimaryActivationInterval)
	{
		return;
	}
	LastPrimaryActivationTime = CurrentTime;

	RequestPrimaryAttack();
}

void ALB_PlayerController::RequestPrimaryAttack()
{
	if (HasAuthority())
	{
		ActivateAbility(LBTags::LBAbilities::Primary);
		return;
	}

	ServerActivateAbility(LBTags::LBAbilities::Primary);
}

void ALB_PlayerController::ApplyInputMappingContexts() const
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!IsValid(LocalPlayer)) return;

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);
	if (!IsValid(InputSubsystem)) return;

	for (UInputMappingContext* Context : InputMappingContexts)
	{
		if (IsValid(Context))
		{
			// 여러 시점에서 재호출되어도 같은 컨텍스트를 0번 우선순위로 유지한다.
			InputSubsystem->AddMappingContext(Context, 0);
		}
	}
}

bool ALB_PlayerController::ActivateAbility(const FGameplayTag& AbilityTag) const
{
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PlayerState.Get());
	if (!IsValid(ASC))
	{
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

void ALB_PlayerController::ServerActivateAbility_Implementation(FGameplayTag AbilityTag)
{
	ActivateAbility(AbilityTag);
}

void ALB_PlayerController::InitializeRaidHUD()
{
	if (!IsLocalController())
	{
		return;
	}

	if (IsValid(RaidHUDWidget))
	{
		return;
	}

	if (!RaidHUDWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LB UI] RaidHUDWidgetClass is not set. Controller=%s"), *GetNameSafe(this));
		return;
	}

	RaidHUDWidget = CreateWidget<ULB_RaidHUDWidget>(this, RaidHUDWidgetClass);
	if (!IsValid(RaidHUDWidget))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LB UI] Failed to create RaidHUDWidget. Controller=%s"), *GetNameSafe(this));
		return;
	}

	// UI는 각 로컬 PlayerController가 자기 화면에 붙인다.
	// GameMode는 서버에만 있으므로 클라이언트 화면 UI를 만들면 안 된다.
	RaidHUDWidget->AddToViewport(RaidHUDWidgetZOrder);
	RaidHUDWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void ALB_PlayerController::RemoveRaidHUD()
{
	if (IsValid(RaidHUDWidget))
	{
		RaidHUDWidget->RemoveFromParent();
		RaidHUDWidget = nullptr;
	}
}

void ALB_PlayerController::ClientInitializeRaidHUD_Implementation()
{
	InitializeRaidHUD();
}