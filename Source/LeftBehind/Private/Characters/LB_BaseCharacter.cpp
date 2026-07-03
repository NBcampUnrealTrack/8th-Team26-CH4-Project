// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/LB_BaseCharacter.h"

#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
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
}

void ALB_BaseCharacter::OnHealthChanged(const FOnAttributeChangeData& AttributeChangeData)
{
	if (AttributeChangeData.NewValue <= 0.f)
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
	
	if (IsValid(GEngine))
	{
		GEngine->AddOnScreenDebugMessage(-1,3.f,FColor::Red,FString::Printf(TEXT("%s has died"),*GetName()));
	}
}

void ALB_BaseCharacter::HandleRespon()
{
	bAlive = true;
}



