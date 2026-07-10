// LB_AttributeSet.cpp

#include "AbilitySystem/LB_AttributeSet.h"

#include "AbilitySystemComponent.h"
#include "Characters/Boss/LB_BossCharacter.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameState/LB_RaidGameState.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "Player/LB_PlayerState.h"
#include "System/Raid/LBRaidBossBase.h"

namespace
{
	ALB_PlayerState* ResolveLBPlayerState(AActor* Actor)
	{
		if (ALB_PlayerState* PlayerState = Cast<ALB_PlayerState>(Actor))
		{
			return PlayerState;
		}

		if (APawn* Pawn = Cast<APawn>(Actor))
		{
			return Pawn->GetPlayerState<ALB_PlayerState>();
		}

		if (AController* Controller = Cast<AController>(Actor))
		{
			return Controller->GetPlayerState<ALB_PlayerState>();
		}

		return nullptr;
	}

	ALB_PlayerState* ResolveSourcePlayerState(const FGameplayEffectContextHandle& Context)
	{
		if (UAbilitySystemComponent* SourceASC = Context.GetOriginalInstigatorAbilitySystemComponent())
		{
			if (ALB_PlayerState* PlayerState = ResolveLBPlayerState(SourceASC->GetOwnerActor()))
			{
				return PlayerState;
			}

			if (ALB_PlayerState* PlayerState = ResolveLBPlayerState(SourceASC->GetAvatarActor()))
			{
				return PlayerState;
			}
		}

		return ResolveLBPlayerState(Context.GetOriginalInstigator());
	}

	bool IsRaidBoss(const AActor* Actor)
	{
		return IsValid(Cast<ALB_BossCharacter>(Actor)) || IsValid(Cast<ALBRaidBossBase>(Actor));
	}
}

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

bool ULB_AttributeSet::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
	if (!Super::PreGameplayEffectExecute(Data))
	{
		return false;
	}

	if (Data.EvaluatedData.Attribute != GetHealthAttribute())
	{
		return true;
	}

	// Health GameplayEffects in this project are additive. Clamp their magnitude before GAS applies it so
	// damage/healing events and raid statistics both use the real HP change (no overkill or overheal).
	if (Data.EvaluatedData.ModifierOp != EGameplayModOp::Additive)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[LB Stats] Non-additive Health GameplayEffect is not included in raid statistics. Effect=%s Op=%d"),
			*GetNameSafe(Data.EffectSpec.Def),
			static_cast<int32>(Data.EvaluatedData.ModifierOp.GetValue()));
		return true;
	}

	const float RawDelta = Data.EvaluatedData.Magnitude;
	const float OldHealth = GetHealth();
	if (!FMath::IsFinite(RawDelta) || !FMath::IsFinite(OldHealth))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[LB Stats] Ignoring invalid Health delta. Target=%s Delta=%f Health=%f"),
			*GetNameSafe(Data.Target.GetAvatarActor()),
			RawDelta,
			OldHealth);
		Data.EvaluatedData.Magnitude = 0.f;
		return true;
	}

	const float CurrentMaxHealth = GetMaxHealth();
	const float UnclampedHealth = OldHealth + RawDelta;
	const float NewHealth = CurrentMaxHealth > 0.f
		? FMath::Clamp(UnclampedHealth, 0.f, CurrentMaxHealth)
		: FMath::Max(0.f, UnclampedHealth);
	const float EffectiveDelta = NewHealth - OldHealth;

	// PostGameplayEffectExecute can now broadcast the effective amount without transient shared state.
	Data.EvaluatedData.Magnitude = EffectiveDelta;

	AActor* TargetOwner = Data.Target.GetOwnerActor();
	UWorld* World = IsValid(TargetOwner) ? TargetOwner->GetWorld() : Data.Target.GetWorld();
	const ALB_RaidGameState* RaidGameState = IsValid(World) ? World->GetGameState<ALB_RaidGameState>() : nullptr;
	if (!IsValid(TargetOwner)
		|| !TargetOwner->HasAuthority()
		|| !IsValid(RaidGameState)
		|| RaidGameState->RaidState != ELBRaidState::Battle
		|| FMath::IsNearlyZero(EffectiveDelta))
	{
		return true;
	}

	const FGameplayEffectContextHandle Context = Data.EffectSpec.GetContext();
	ALB_PlayerState* SourcePlayerState = ResolveSourcePlayerState(Context);
	if (!IsValid(SourcePlayerState))
	{
		return true;
	}

	AActor* TargetAvatar = Data.Target.GetAvatarActor();
	if (EffectiveDelta < 0.f && IsRaidBoss(TargetAvatar))
	{
		// This runs before the target Health delegate, so a lethal hit is recorded before EndRaid.
		SourcePlayerState->AddTotalDamageDealt_ServerOnly(-EffectiveDelta);
	}
	else if (EffectiveDelta > 0.f)
	{
		ALB_PlayerState* TargetPlayerState = ResolveLBPlayerState(TargetOwner);
		if (!IsValid(TargetPlayerState))
		{
			TargetPlayerState = ResolveLBPlayerState(TargetAvatar);
		}

		// In a raid, two distinct LB PlayerStates represent non-self healing of a party member.
		if (IsValid(TargetPlayerState) && TargetPlayerState != SourcePlayerState)
		{
			SourcePlayerState->AddTotalHealingDone_ServerOnly(EffectiveDelta);
		}
	}

	return true;
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
		const float EffectiveDelta = Data.EvaluatedData.Magnitude;
		
		
		UE_LOG(LogTemp, Warning, TEXT("HP Changed"));
		const FGameplayEffectContextHandle Context = Data.EffectSpec.GetContext();
		AActor* Instigator = Context.GetOriginalInstigator();
		AActor* Causer = Context.GetEffectCauser();
		
		
		if (EffectiveDelta < 0.f)
		{
			const float Damage = FMath::Abs(EffectiveDelta);
			ActorDamaged.Broadcast(Instigator,Causer,Damage);
		}
		else if (EffectiveDelta > 0.f)
		{
			ActorHealed.Broadcast(Instigator,Causer,EffectiveDelta);
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
