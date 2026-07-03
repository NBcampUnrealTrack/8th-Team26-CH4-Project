// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/LB_AttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

void ULB_AttributeSet::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, Mana, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, MaxMana, COND_None, REPNOTIFY_Always);
	
	DOREPLIFETIME(ThisClass, bAttributeInitialized);
	
}

void ULB_AttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// 화면에 보이는 수치는 항상 0~최대값 사이여야 UI와 사망 판정이 흔들리지 않는다.
	if (Attribute == GetHealthAttribute())
	{
		const float CurrentMaxHealth = GetMaxHealth();
		NewValue = CurrentMaxHealth > 0.f ? FMath::Clamp(NewValue, 0.f, CurrentMaxHealth) : FMath::Max(0.f, NewValue);
	}
	else if (Attribute == GetManaAttribute())
	{
		const float CurrentMaxMana = GetMaxMana();
		NewValue = CurrentMaxMana > 0.f ? FMath::Clamp(NewValue, 0.f, CurrentMaxMana) : FMath::Max(0.f, NewValue);
	}
	else if (Attribute == GetMaxHealthAttribute() || Attribute == GetMaxManaAttribute())
	{
		NewValue = FMath::Max(1.f, NewValue);
	}
}

void ULB_AttributeSet::PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		const float CurrentMaxHealth = GetMaxHealth();
		SetHealth(CurrentMaxHealth > 0.f ? FMath::Clamp(GetHealth(), 0.f, CurrentMaxHealth) : FMath::Max(0.f, GetHealth()));
	}
	else if (Data.EvaluatedData.Attribute == GetManaAttribute())
	{
		const float CurrentMaxMana = GetMaxMana();
		SetMana(CurrentMaxMana > 0.f ? FMath::Clamp(GetMana(), 0.f, CurrentMaxMana) : FMath::Max(0.f, GetMana()));
	}
	else if (Data.EvaluatedData.Attribute == GetMaxHealthAttribute())
	{
		SetMaxHealth(FMath::Max(1.f, GetMaxHealth()));
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
	}
	else if (Data.EvaluatedData.Attribute == GetMaxManaAttribute())
	{
		SetMaxMana(FMath::Max(1.f, GetMaxMana()));
		SetMana(FMath::Clamp(GetMana(), 0.f, GetMaxMana()));
	}
	
	if (!bAttributeInitialized)
	{
		bAttributeInitialized = true;
		OnAttributesInitialized.Broadcast();
	}
}

void ULB_AttributeSet::OnRep_AttributesInitalized()
{
	
	if (bAttributeInitialized)
	{
		OnAttributesInitialized.Broadcast();
	}
}

void ULB_AttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, Health, OldValue);
}

void ULB_AttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, MaxHealth, OldValue);
}

void ULB_AttributeSet::OnRep_Mana(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, Mana, OldValue);
}

void ULB_AttributeSet::OnRep_MaxMana(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, MaxMana, OldValue);
}
