// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/LB_PlayerController.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "GameState/LB_RaidGameState.h"
#include "GameplayTags/LBTags.h"
#include "TimerManager.h"
#include "UI/LB_AttributeWidget.h"
#include "UI/Popup/LB_RaidResultWidget.h"
#include "UObject/ConstructorHelpers.h"

ALB_PlayerController::ALB_PlayerController()
{
	// 화면 상단 보스 HP는 기존 WBP_HealthBar를 기본값으로 사용한다.
	static ConstructorHelpers::FClassFinder<ULB_AttributeWidget> BossHPWidgetClassFinder(
		TEXT("/Game/LeftBehind/UI/WBP_HealthBar"));
	if (BossHPWidgetClassFinder.Succeeded())
	{
		BossHPWidgetClass = BossHPWidgetClassFinder.Class;
	}

	// BP 결과 화면이 아직 없어도 보스 처치 승리를 바로 확인할 수 있는 C++ 기본 위젯을 사용한다.
	RaidVictoryWidgetClass = ULB_RaidResultWidget::StaticClass();
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
		EnhancedInputComponent->BindAction(PrimaryAction, ETriggerEvent::Started, this, &ThisClass::Primary);
	}
}

void ALB_PlayerController::BeginPlay()
{
	Super::BeginPlay();

	ApplyInputMappingContexts();
	InitializeBossHPWidget();

	if (HasAuthority())
	{
		// Listen Server에서는 원격 클라이언트 화면 UI도 각 클라이언트가 직접 만들어야 한다.
		ClientInitializeBossHPWidget();
	}
}

void ALB_PlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BossHPWidgetBindRetryTimerHandle);
	}

	if (IsValid(RaidVictoryWidget))
	{
		RaidVictoryWidget->RemoveFromParent();
		RaidVictoryWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void ALB_PlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);

	ApplyInputMappingContexts();
	InitializeBossHPWidget();

	if (HasAuthority())
	{
		// Possess 이후에 들어오는 클라이언트도 보스 HP HUD를 놓치지 않게 한 번 더 보장한다.
		ClientInitializeBossHPWidget();
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

void ALB_PlayerController::ClientInitializeBossHPWidget_Implementation()
{
	InitializeBossHPWidget();
}

void ALB_PlayerController::InitializeBossHPWidget()
{
	if (!IsLocalController())
	{
		return;
	}

	if (!BossHPWidgetClass)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[LB UI] BossHPWidgetClass is not set. Controller=%s"),
			*GetNameSafe(this)
		);
		return;
	}

	if (!IsValid(BossHPWidget))
	{
		BossHPWidget = CreateWidget<ULB_AttributeWidget>(this, BossHPWidgetClass);
		if (IsValid(BossHPWidget))
		{
			BossHPWidget->AddToViewport(BossHPWidgetZOrder);
			RefreshBossHPWidgetLayout();
			BossHPWidget->SetVisibility(ESlateVisibility::Collapsed);
			// 보스가 생성되기 전까지는 빈 값으로 시작하고, GameState HP가 들어오면 즉시 갱신된다.
			BossHPWidget->SetAttributeValues(0.f, 1.f);
		}
	}

	if (!BindBossHPWidgetToGameState())
	{
		if (UWorld* World = GetWorld())
		{
			// 클라이언트는 GameState 복제가 늦게 도착할 수 있으므로 짧게 재시도한다.
			World->GetTimerManager().SetTimer(
				BossHPWidgetBindRetryTimerHandle,
				this,
				&ThisClass::InitializeBossHPWidget,
				0.25f,
				false
			);
		}
	}
}

bool ALB_PlayerController::BindBossHPWidgetToGameState()
{
	if (!IsLocalController() || !IsValid(BossHPWidget))
	{
		return false;
	}

	ALB_RaidGameState* RaidGameState = GetWorld() ? GetWorld()->GetGameState<ALB_RaidGameState>() : nullptr;
	if (!IsValid(RaidGameState))
	{
		return false;
	}

	RaidGameState->OnBossHPChanged.AddUniqueDynamic(this, &ThisClass::HandleBossHPChanged);
	RaidGameState->OnRaidStateChanged.AddUniqueDynamic(this, &ThisClass::HandleRaidStateChanged);
	RaidGameState->OnRaidResultChanged.AddUniqueDynamic(this, &ThisClass::HandleRaidResultChanged);

	HandleBossHPChanged(RaidGameState->BossCurrentHP, RaidGameState->BossMaxHP);
	HandleRaidStateChanged(RaidGameState->RaidState);
	HandleRaidResultChanged(RaidGameState->RaidResult);
	return true;
}

void ALB_PlayerController::RefreshBossHPWidgetLayout() const
{
	if (!IsValid(BossHPWidget))
	{
		return;
	}

	int32 ViewportX = 0;
	int32 ViewportY = 0;
	GetViewportSize(ViewportX, ViewportY);

	// 보스 월드 위치가 아니라, 각 클라이언트의 화면(Viewport) 기준 중앙에 고정한다.
	// bRemoveDPIScale=true를 써야 PIE 창 크기나 UI DPI 스케일이 달라도 실제 화면 픽셀 중앙에 맞는다.
	const float CenterX = ViewportX > 0 ? ViewportX * 0.5f : 0.f;
	BossHPWidget->SetAlignmentInViewport(FVector2D(0.5f, 0.f));
	BossHPWidget->SetPositionInViewport(FVector2D(CenterX, BossHPWidgetTopOffset), true);
	BossHPWidget->SetDesiredSizeInViewport(BossHPWidgetSize);
}

void ALB_PlayerController::UpdateBossHPWidgetVisibility(ELBRaidState RaidState) const
{
	if (!IsValid(BossHPWidget))
	{
		return;
	}

	const bool bShouldShow =
		!bShowBossHPOnlyInBattle
		|| RaidState == ELBRaidState::Battle
		|| RaidState == ELBRaidState::Result;

	BossHPWidget->SetVisibility(bShouldShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void ALB_PlayerController::HandleBossHPChanged(float CurrentHP, float MaxHP)
{
	if (!IsLocalController() || !IsValid(BossHPWidget))
	{
		return;
	}

	RefreshBossHPWidgetLayout();
	BossHPWidget->SetAttributeValues(CurrentHP, MaxHP);
}

void ALB_PlayerController::HandleRaidStateChanged(ELBRaidState NewState)
{
	UpdateBossHPWidgetVisibility(NewState);
	RefreshRaidVictoryWidget();
}

void ALB_PlayerController::HandleRaidResultChanged(const FLBRaidResultData& ResultData)
{
	(void)ResultData;
	RefreshRaidVictoryWidget();
}

void ALB_PlayerController::RefreshRaidVictoryWidget()
{
	if (!IsLocalController())
	{
		return;
	}

	const ALB_RaidGameState* RaidGameState = GetWorld() ? GetWorld()->GetGameState<ALB_RaidGameState>() : nullptr;
	const bool bShouldShowVictory =
		IsValid(RaidGameState)
		&& RaidGameState->RaidState == ELBRaidState::Result
		&& RaidGameState->RaidResult.bVictory
		&& RaidGameState->RaidResult.EndReason == ELBRaidEndReason::BossKilled;

	if (!bShouldShowVictory)
	{
		HideRaidVictoryWidget();
		return;
	}

	ShowRaidVictoryWidget(RaidGameState->RaidResult);
}

void ALB_PlayerController::ShowRaidVictoryWidget(const FLBRaidResultData& ResultData)
{
	if (!IsLocalController())
	{
		return;
	}

	if (!RaidVictoryWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LB UI] RaidVictoryWidgetClass is not set. Controller=%s"), *GetNameSafe(this));
		return;
	}

	if (!IsValid(RaidVictoryWidget))
	{
		RaidVictoryWidget = CreateWidget<ULB_RaidResultWidget>(this, RaidVictoryWidgetClass);
		if (IsValid(RaidVictoryWidget))
		{
			RaidVictoryWidget->AddToViewport(RaidVictoryWidgetZOrder);
		}
	}

	if (IsValid(RaidVictoryWidget))
	{
		// 결과 데이터는 서버에서 확정되어 GameState로 복제된 값만 사용한다.
		RaidVictoryWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void ALB_PlayerController::HideRaidVictoryWidget() const
{
	if (IsValid(RaidVictoryWidget))
	{
		RaidVictoryWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}
