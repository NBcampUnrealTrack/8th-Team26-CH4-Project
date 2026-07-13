// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/LB_BaseCharacter.h"

#include "AbilitySystem/LB_AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameplayAbilitySpec.h"
#include "Components/SkeletalMeshComponent.h"
#include "Net/UnrealNetwork.h"


// Sets default values
ALB_BaseCharacter::ALB_BaseCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}

void ALB_BaseCharacter::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ThisClass,bAlive);
}

UAbilitySystemComponent* ALB_BaseCharacter::GetAbilitySystemComponent() const
{
	return nullptr;
}

UAttributeSet* ALB_BaseCharacter::GetAttributeSet() const
{
	return nullptr;
}

void ALB_BaseCharacter::MulticastPlayCosmeticMontage_Implementation(UAnimMontage* Montage, float PlayRate)
{
	if (!IsValid(Montage))
	{
		return;
	}

	USkeletalMeshComponent* MeshComponent = GetMesh();
	UAnimInstance* AnimInstance = IsValid(MeshComponent) ? MeshComponent->GetAnimInstance() : nullptr;
	if (!IsValid(AnimInstance))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LB Animation] Montage play failed. Character=%s Montage=%s"),
			*GetNameSafe(this),
			*GetNameSafe(Montage));
		return;
	}

	// 공격 판정은 서버에서 이미 처리하고, 몽타주는 모든 클라이언트가 같은 타이밍에 보도록 재생한다.
	AnimInstance->Montage_Play(Montage, FMath::Max(0.01f, PlayRate));
}

void ALB_BaseCharacter::GiveStartupAbilities()
{
	if (!HasAuthority()) return;

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!IsValid(ASC)) return;

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : StartupAbilities)
	{
		if (!AbilityClass) continue;

		bool bAlreadyGranted = false;
		for (const FGameplayAbilitySpec& ExistingSpec : ASC->GetActivatableAbilities())
		{
			if (ExistingSpec.Ability && ExistingSpec.Ability->GetClass() == AbilityClass)
			{
				bAlreadyGranted = true;
				break;
			}
		}

		if (bAlreadyGranted)
		{
			continue;
		}

		// 서버에서 한 번만 능력을 지급하면 ASC 복제로 각 클라이언트가 같은 능력 목록을 받는다.
		ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
	}
}

void ALB_BaseCharacter::InitializeAttribute() const
{
	if (!HasAuthority()) return;

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!IsValid(ASC)) return;

	if (!IsValid(InitializeAttributesEffect))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] InitializeAttributesEffect is not set. 기본 스탯 적용을 건너뜁니다."), *GetName());
		if (ULB_AttributeSet* LBAttributeSet = Cast<ULB_AttributeSet>(GetAttributeSet()))
		{
			// Max 값이 비어 있어도 시작 체력/마나가 0으로 보이지 않도록 최소값으로 채운다.
			LBAttributeSet->FillCurrentAttributesToMax();
		}
		return;
	}

	FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
	ContextHandle.AddSourceObject(this);

	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(InitializeAttributesEffect, 1.f, ContextHandle);
	if (!SpecHandle.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] InitializeAttributesEffect spec 생성에 실패했습니다."), *GetName());
		return;
	}

	ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

	if (ULB_AttributeSet* LBAttributeSet = Cast<ULB_AttributeSet>(GetAttributeSet()))
	{
		// GE가 MaxHealth/MaxMana만 세팅해도 실제 시작값은 항상 최대치로 맞춘다.
		LBAttributeSet->FillCurrentAttributesToMax();
	}
}

void ALB_BaseCharacter::OnHealthChanged(const FOnAttributeChangeData& AttributeChangeData)
{
	const ULB_AttributeSet* LBAttributeSet = Cast<ULB_AttributeSet>(GetAttributeSet());
	if (!IsValid(LBAttributeSet) || !LBAttributeSet->bAttributeInitialized || LBAttributeSet->GetMaxHealth() <= 0.f)
	{
		return;
	}

	// 초기화 중 0 -> 0 같은 값은 죽음으로 보지 않고, 살아 있던 대상이 0 이하가 된 순간만 사망 처리한다.
	if (bAlive && AttributeChangeData.OldValue > 0.f && AttributeChangeData.NewValue <= 0.f)
	{
		HandleDeath();
	}
}

void ALB_BaseCharacter::HandleDeath()
{
	if (!bAlive)
	{
		return;
	}

	bAlive = false;

	UE_LOG(LogTemp, Warning, TEXT("[LB Character] %s has died"), *GetName());

	if (HasAuthority())
	{
		MulticastPlayCosmeticMontage(DeathMontage);
	}
}

void ALB_BaseCharacter::HandleRespon()
{
	bAlive = true;
}



