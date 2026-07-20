// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/LB_PlayerCharacter.h"

#include "AbilitySystem/LB_AttributeSet.h"
#include "Camera/CameraComponent.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameMode/LB_RaidGameMode.h"
#include "Player/LB_PlayerState.h"

namespace
{
	constexpr float SharedCameraArmLength = 800.f;
	const FVector SharedCameraTargetOffset(0.f, 60.f, 180.f);
}


// Sets default values
ALB_PlayerCharacter::ALB_PlayerCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);
	
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f);
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.f;
	
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>("CameraBoom");
	CameraBoom->SetupAttachment(GetRootComponent());
	ApplySharedCameraBoomSettings();
	CameraBoom->bUsePawnControlRotation = true;
	
	FollowCamera = CreateDefaultSubobject<UCameraComponent>("FollowCamera");
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void ALB_PlayerCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Camera components are not replicated, so apply the same final settings to every
	// local character instance after Blueprint component defaults have been loaded.
	ApplySharedCameraBoomSettings();
}

void ALB_PlayerCharacter::ApplySharedCameraBoomSettings()
{
	if (!IsValid(CameraBoom))
	{
		return;
	}

	CameraBoom->TargetArmLength = SharedCameraArmLength;
	CameraBoom->TargetOffset = SharedCameraTargetOffset;
}

void ALB_PlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	UE_LOG(LogTemp, Warning,
		TEXT("[Character BeginPlay] %s"),
		*GetName());

	UE_LOG(LogTemp, Warning,
		TEXT("  MeshClass = %s"),
		*GetClass()->GetName());

	UE_LOG(LogTemp, Warning,
		TEXT("  BoomLength = %.1f"),
		CameraBoom->TargetArmLength);

	UE_LOG(LogTemp, Warning,
		TEXT("  BoomRotation = %s"),
		*CameraBoom->GetRelativeRotation().ToString());

	UE_LOG(LogTemp, Warning,
		TEXT("  CameraRelative = %s"),
		*FollowCamera->GetRelativeLocation().ToString());
}

UAbilitySystemComponent* ALB_PlayerCharacter::GetAbilitySystemComponent() const
{
	ALB_PlayerState* LBPlayerState = Cast<ALB_PlayerState>(GetPlayerState());
	if (!IsValid(LBPlayerState) && IsValid(GetController()))
	{
		// Pawn의 PlayerState 복제가 아직 늦게 들어온 순간에는 Controller가 먼저 알고 있을 수 있다.
		LBPlayerState = GetController()->GetPlayerState<ALB_PlayerState>();
	}
	if (!IsValid(LBPlayerState)) return nullptr;
	
	return LBPlayerState->GetAbilitySystemComponent();
}

UAttributeSet* ALB_PlayerCharacter::GetAttributeSet() const
{
	const ALB_PlayerState* LBPlayerState = Cast<ALB_PlayerState>(GetPlayerState());
	if (!IsValid(LBPlayerState) && IsValid(GetController()))
	{
		// AttributeSet도 ASC와 같은 PlayerState에서 가져와 플레이어 스탯 원본을 하나로 유지한다.
		LBPlayerState = GetController()->GetPlayerState<ALB_PlayerState>();
	}
	if (!IsValid(LBPlayerState)) return nullptr;

	return LBPlayerState->GetLBAttributeSet();
}

void ALB_PlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	InitializeAbilityActorInfo();
}

void ALB_PlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	UE_LOG(LogTemp, Warning,
		TEXT("[Character] OnRep_PlayerState %s"),
		*GetName());
	
	InitializeAbilityActorInfo();
}

void ALB_PlayerCharacter::InitializeAbilityActorInfo()
{
	ALB_PlayerState* LBPlayerState = Cast<ALB_PlayerState>(GetPlayerState());
	if (!IsValid(LBPlayerState)) return;

	UAbilitySystemComponent* ASC = LBPlayerState->GetAbilitySystemComponent();
	if (!IsValid(ASC)) return;

	// PlayerState는 능력의 주인, Character는 실제 몸이다. 이 둘을 알려줘야 GAS가 멀티에서 대상을 정확히 찾는다.
	ASC->InitAbilityActorInfo(LBPlayerState, this);
	OnAscInitialized.Broadcast(ASC, LBPlayerState->GetLBAttributeSet());

	if (!HasAuthority())
	{
		return;
	}

	// 능력 부여와 기본 스탯 적용은 서버만 한다. 그래야 클라이언트가 체력/마나를 마음대로 바꿀 수 없다.
	GiveStartupAbilities();
	InitializeAttribute();
	BindHealthChangedDelegate();
}

void ALB_PlayerCharacter::BindHealthChangedDelegate()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	ULB_AttributeSet* LBAttributeSet = Cast<ULB_AttributeSet>(GetAttributeSet());
	if (!IsValid(ASC) || !IsValid(LBAttributeSet)) return;

	ASC->GetGameplayAttributeValueChangeDelegate(LBAttributeSet->GetHealthAttribute()).RemoveAll(this);
	ASC->GetGameplayAttributeValueChangeDelegate(LBAttributeSet->GetHealthAttribute()).AddUObject(this, &ThisClass::OnHealthChanged);
}

void ALB_PlayerCharacter::HandleDeath()
{
	if (!IsAlive())
	{
		return;
	}

	Super::HandleDeath();

	if (!HasAuthority()) return;

	if (ALB_RaidGameMode* RaidGameMode = GetWorld()->GetAuthGameMode<ALB_RaidGameMode>())
	{
		RaidGameMode->NotifyPlayerDied(GetController());
	}
}

