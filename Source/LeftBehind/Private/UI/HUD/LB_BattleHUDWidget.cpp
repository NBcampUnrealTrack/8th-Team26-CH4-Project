// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/HUD/LB_BattleHUDWidget.h"
#include "UI/Panels/LB_BossHPWidget.h"
#include "Characters/LB_PlayerCharacter.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/LB_AttributeSet.h"

void ULB_BattleHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindPlayerAttributes();
}

void ULB_BattleHUDWidget::NativeDestruct()
{
	GetWorld()->GetTimerManager().ClearTimer(DamagePriorityTimer);
	UnbindPlayerAttributes();
	Super::NativeDestruct();
}

void ULB_BattleHUDWidget::BindPlayerAttributes()
{
	ALB_PlayerCharacter* PlayerCharacter =
		Cast<ALB_PlayerCharacter>(GetOwningPlayerPawn());

	if (!PlayerCharacter)
	{
		return;
	}

	CachedASC = PlayerCharacter->GetAbilitySystemComponent();
	CachedAttributeSet =
		Cast<ULB_AttributeSet>(PlayerCharacter->GetAttributeSet());

	if (!CachedASC || !CachedAttributeSet)
	{
		return;
	}

	HealthChangedHandle =
		CachedASC
		->GetGameplayAttributeValueChangeDelegate(
			CachedAttributeSet->GetHealthAttribute())
		.AddUObject(this, &ThisClass::OnHealthChanged);
}

void ULB_BattleHUDWidget::UnbindPlayerAttributes()
{
	if (!CachedASC || !CachedAttributeSet)
	{
		return;
	}

	CachedASC
		->GetGameplayAttributeValueChangeDelegate(
			CachedAttributeSet->GetHealthAttribute())
		.Remove(HealthChangedHandle);

	CachedASC = nullptr;
	CachedAttributeSet = nullptr;
}

void ULB_BattleHUDWidget::OnHealthChanged(const struct FOnAttributeChangeData& Data)
{
	const float Delta = Data.NewValue - Data.OldValue;

	if (Delta < 0.f)
	{
		const float Damage = -Delta;

		BP_PlayDamageEffect();

		bDamageEffectPlaying = true;

		GetWorld()->GetTimerManager().ClearTimer(DamagePriorityTimer);

		GetWorld()->GetTimerManager().SetTimer(
			DamagePriorityTimer,
			this,
			&ThisClass::ClearDamagePriority,
			0.25f,
			false);

		return;
	}

	if (Delta > 0.f)
	{
		const float Heal = Delta;

		if (!bDamageEffectPlaying)
		{
			BP_PlayHealEffect();
		}
	}
}

void ULB_BattleHUDWidget::HandleBossHPChanged(float CurrentHP, float MaxHP)
{
	Super::HandleBossHPChanged(CurrentHP, MaxHP);
	
	if (BossHPWidget)
	{
		BossHPWidget->SetBossHP(CurrentHP, MaxHP);
	}
}

void ULB_BattleHUDWidget::ClearDamagePriority()
{
	bDamageEffectPlaying = false;
}
