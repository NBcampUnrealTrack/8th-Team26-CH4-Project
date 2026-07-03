// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/LB_PlayerController.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GameplayTags/LBTags.h"


void ALB_PlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!IsValid(LocalPlayer)) return;

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);
	if (!IsValid(InputSubsystem)) return;

	for (UInputMappingContext* Context : InputMappingContexts)
	{
		if (IsValid(Context))
		{
			InputSubsystem->AddMappingContext(Context, 0);
		}
	}

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (!IsValid(EnhancedInputComponent)) return;

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
		// Started는 버튼을 누른 첫 순간만 호출된다. 공격 능력이 프레임마다 반복 발동되는 것을 막는다.
		EnhancedInputComponent->BindAction(PrimaryAction, ETriggerEvent::Started, this, &ThisClass::Primary);
	}
}

void ALB_PlayerController::BeginPlay()
{
	Super::BeginPlay();
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
	ActivateAbility(LBTags::LBAbilities::Primary);
}

void ALB_PlayerController::ActivateAbility(const FGameplayTag& AbilityTag) const
{
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetPawn());
	if (!IsValid(ASC)) return;

	// 클라이언트가 입력을 보내면 ASC가 예측 활성화를 시도하고, 서버가 최종 권한으로 확정한다.
	ASC->TryActivateAbilitiesByTag(AbilityTag.GetSingleTagContainer());
}
