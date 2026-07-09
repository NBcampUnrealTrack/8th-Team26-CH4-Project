// LB_AttributeSet.cpp

#include "AbilitySystem/LB_AttributeSet.h"

#include "AbilitySystemComponent.h"
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

	// 화면에 보이는 현재 HP/Mana는 항상 0~Max 범위 안에 있어야 한다.
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
		// Max 값이 0이면 UI 비율 계산과 사망 판정이 꼬이므로 최소 1을 보장한다.
		NewValue = FMath::Max(1.f, NewValue);
	}
}

void ULB_AttributeSet::FillCurrentAttributesToMax()
{
	// 데이터 에셋에서 Max 값만 들어와도 시작 시에는 현재 HP/Mana를 항상 최대치로 채운다.
	const float SafeMaxHealth = FMath::Max(1.f, GetMaxHealth());
	const float SafeMaxMana = FMath::Max(1.f, GetMaxMana());

	if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
	{
		// ASC를 통해 값을 넣으면 Attribute 변경 델리게이트가 흘러 UI도 즉시 갱신된다.
		ASC->SetNumericAttributeBase(GetMaxHealthAttribute(), SafeMaxHealth);
		ASC->SetNumericAttributeBase(GetMaxManaAttribute(), SafeMaxMana);
		ASC->SetNumericAttributeBase(GetHealthAttribute(), SafeMaxHealth);
		ASC->SetNumericAttributeBase(GetManaAttribute(), SafeMaxMana);
	}
	else
	{
		SetMaxHealth(SafeMaxHealth);
		SetMaxMana(SafeMaxMana);
		SetHealth(SafeMaxHealth);
		SetMana(SafeMaxMana);
	}

	if (!bAttributeInitialized)
	{
		bAttributeInitialized = true;
	}

	OnAttributesInitialized.Broadcast();
}

void ULB_AttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
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

	//ActorDamage는 데미지 입력시에 확인 용도
	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		const float RawDelta = Data.EvaluatedData.Magnitude;
		
		
		UE_LOG(LogTemp, Warning, TEXT("HP Changed"));
		const FGameplayEffectContextHandle Context = Data.EffectSpec.GetContext();
		AActor* Instigator = Context.GetOriginalInstigator();
		AActor* Causer = Context.GetEffectCauser();
		
		
		if (RawDelta < 0.f)
		{
			const float Damage = FMath::Abs(RawDelta);
			ActorDamaged.Broadcast(Instigator,Causer,Damage);
		}
		else
		{
			ActorHealed.Broadcast(Instigator,Causer,RawDelta);
		}

		
	}


	// 초기화 완료 방송은 FillCurrentAttributesToMax()에서만 한다.
	// GE가 MaxHealth만 먼저 적용한 중간 상태를 UI가 "초기화 완료"로 오해하지 않게 하기 위함이다.

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
