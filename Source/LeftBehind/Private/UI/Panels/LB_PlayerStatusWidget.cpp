// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Panels/LB_PlayerStatusWidget.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/LB_AttributeSet.h"
#include "Characters/LB_PlayerCharacter.h"
#include "Player/LB_PlayerState.h"

void ULB_PlayerStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	BindPlayerAttributes();
	RefreshStatus();
	RefreshPortrait();
}

void ULB_PlayerStatusWidget::NativeDestruct()
{
	UnbindPlayerAttributes();
	
	Super::NativeDestruct();
}

void ULB_PlayerStatusWidget::BindPlayerAttributes()
{
	ALB_PlayerCharacter* PlayerCharacter = Cast<ALB_PlayerCharacter>(GetOwningPlayerPawn());
	if (!IsValid(PlayerCharacter)) return;
	
	CachedASC = PlayerCharacter->GetAbilitySystemComponent();
	CachedAttributeSet = Cast<ULB_AttributeSet>(PlayerCharacter->GetAttributeSet());
	if (!IsValid(CachedASC) || !IsValid(CachedAttributeSet)) return;
	
	HealthChangedHandle =
		CachedASC->GetGameplayAttributeValueChangeDelegate(
			CachedAttributeSet->GetHealthAttribute())
		.AddUObject(this, &ThisClass::OnHealthChanged);

	ManaChangedHandle =
		CachedASC->GetGameplayAttributeValueChangeDelegate(
			CachedAttributeSet->GetManaAttribute())
		.AddUObject(this, &ThisClass::OnManaChanged);
}

void ULB_PlayerStatusWidget::UnbindPlayerAttributes()
{
	if (!IsValid(CachedASC) || !IsValid(CachedAttributeSet)) return;
	
	CachedASC->GetGameplayAttributeValueChangeDelegate(
		CachedAttributeSet->GetHealthAttribute())
		.Remove(HealthChangedHandle);

	CachedASC->GetGameplayAttributeValueChangeDelegate(
		CachedAttributeSet->GetManaAttribute())
		.Remove(ManaChangedHandle);

	CachedASC = nullptr;
	CachedAttributeSet = nullptr;
}

void ULB_PlayerStatusWidget::OnHealthChanged(const struct FOnAttributeChangeData& Data)
{
	RefreshStatus();
}

void ULB_PlayerStatusWidget::OnManaChanged(const struct FOnAttributeChangeData& Data)
{
	RefreshStatus();
}

void ULB_PlayerStatusWidget::RefreshStatus()
{
	if (!IsValid(CachedAttributeSet)) return;
	
	BP_OnHealthChanged(
		CachedAttributeSet->GetHealth(),
		CachedAttributeSet->GetMaxHealth());

	BP_OnManaChanged(
		CachedAttributeSet->GetMana(),
		CachedAttributeSet->GetMaxMana());
}

void ULB_PlayerStatusWidget::RefreshPortrait()
{
	if (!CharacterDataTable)
	{
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	ALB_PlayerState* PS = PC->GetPlayerState<ALB_PlayerState>();
	if (!PS)
	{
		return;
	}

	const FString RowName =
		StaticEnum<ELBCharacterID>()
		->GetNameStringByValue((int64)PS->GetCharacterID());

	const FLBCharacterData* CharacterData =
		CharacterDataTable->FindRow<FLBCharacterData>(
			FName(*RowName),
			TEXT("PlayerStatus"));

	if (!CharacterData)
	{
		return;
	}

	BP_UpdatePortrait(CharacterData->HUDPortraitImage);
}
