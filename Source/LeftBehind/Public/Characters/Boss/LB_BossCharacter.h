// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayTagContainer.h"
#include "Characters/LB_BaseCharacter.h"
#include "GameplayTags/LBTags.h"
#include "LB_BossCharacter.generated.h"

class ULB_AttributeSet;
struct FGameplayAbilitySpecHandle;
class ULB_AttackPatternComponent;
class ULB_ThreatComponent;
class UAttributeSet;
class ULB_AbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLB_BossHPChangedSignature, float, CurrentHP, float, MaxHP);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLB_BossDiedSignature);

// 보스 페이즈 하나의 조건이다. HealthThreshold 이하가 되면 해당 페이즈 태그를 방송한다.
USTRUCT(BlueprintType)
struct FPhaseInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Phase")
	FGameplayTag PhaseTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Phase")
	float HealthThreshold = 0.f;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly, Category = "Boss|Phase")
	TArray<TSubclassOf<UGameplayAbility>> PhaseSkill;
	
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPhaseChange, FGameplayTag, PhaseTag);

UCLASS()
class LEFTBEHIND_API ALB_BossCharacter : public ALB_BaseCharacter
{
	GENERATED_BODY()

public:
	ALB_BossCharacter();
	
	
	virtual void BeginPlay() override;
	
	
	void HandleHealthAttributeChanged(const FOnAttributeChangeData& AttributeChangeData);
	void HandleMaxHealthAttributeChanged(const FOnAttributeChangeData& AttributeChangeData);
	virtual UAttributeSet* GetAttributeSet() const override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION()
	void Die_ServerOnly();
	UFUNCTION()
	void OnRep_CurrentHP();
	
	UPROPERTY(BlueprintAssignable, Category="LB|Boss")
	FOnLB_BossHPChangedSignature OnBossHPChanged;

	UPROPERTY(BlueprintAssignable, Category="LB|Boss")
	FOnLB_BossDiedSignature OnBossDied;

	UFUNCTION(BlueprintCallable, Category="LB|Boss")
	void InitializeBossStats_ServerOnly(float InMaxHP, float InMaxMana, float InDEF);

	UFUNCTION(BlueprintCallable, Category="LB|Boss")
	void ApplyRaidDamage_ServerOnly(float DamageAmount);

	UFUNCTION(BlueprintPure, Category="LB|Boss")
	float GetCurrentHP() const {return CurrentHP;}

	UFUNCTION(BlueprintPure, Category="LB|Boss")
	float GetMaxHP() const {return MaxHP;}

	//Status 중 마나 없음
	/*UFUNCTION(BlueprintPure, Category="LB|Boss")
	float GetCurrentMana() const;

	UFUNCTION(BlueprintPure, Category="LB|Boss")
	float GetMaxMana() const;*/

	UFUNCTION(BlueprintPure, Category="LB|Boss")
	float GetDEF() const { return DEF;}

	UFUNCTION(BlueprintPure, Category="LB|Boss")
	bool IsDead() const { return bIsDead;}

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated)
	bool bIsBeingLaunched{false};
	
	
	// ----- Phase -----
	UPROPERTY(BlueprintAssignable, Category="Boss|Phase")
	FPhaseChange PhaseChange;
	
	UPROPERTY(EditDefaultsOnly, Category = "Boss|Phase")
	TSubclassOf<UGameplayEffect> PhaseTagGrantEffectClass;

	
	void StopMovementUntilLanded();
	
	// ---- Attack ----
	ULB_ThreatComponent* GetThreatComponent() {return ThreatComponent;}
	ULB_AttackPatternComponent* GetAttackComponent() { return AttackPatternComponent;}
	
	//Player에게 접근하기 위한 거리
	UPROPERTY(EditAnywhere,BlueprintReadOnly, Category = "BOSS|AI")
	float MeleeDistance;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly, Category = "BOSS|AI")
	float RangedDistance;

protected:
	
	

	virtual void HandleDeath() override;

	TObjectPtr<ULB_ThreatComponent> ThreatComponent;
	
	TObjectPtr<ULB_AttackPatternComponent> AttackPatternComponent;
	
	
	// ----- Phase -----
	virtual void HandlePaseChanged(const FOnAttributeChangeData& AttributeChangeData);
	virtual int32 CalculatePhase(const FOnAttributeChangeData& AttributeChangeData);
	
	void ApplyPhaseAbilities(int32 PhaseIndex);
	

	//Status
	UPROPERTY(ReplicatedUsing=OnRep_CurrentHP, BlueprintReadOnly, Category="LB|Boss")
	float CurrentHP = 10000.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="LB|Boss")
	float MaxHP = 10000.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="LB|Boss")
	float DEF = 50.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="LB|Boss")
	bool bIsDead = false;

	bool bInitializingStats = false;
	


private:
	UFUNCTION()
	void EnableMovementOnLanded(const FHitResult& Hit);
	
	UPROPERTY()
	TObjectPtr<ULB_AbilitySystemComponent> AbilitySystemComponent;
	
	UPROPERTY()
	TObjectPtr<ULB_AttributeSet> Attributeset;
	
	// ---- Phase ----

	UPROPERTY(EditAnywhere, Category="Boss|Phase", meta=(AllowPrivateAccess=true))
	TArray<FPhaseInfo> PhaseInfos;

	UPROPERTY()
	int32 CurrentPhaseIndex = INDEX_NONE;
	
	UPROPERTY()
	FActiveGameplayEffectHandle CurrentPhaseTagHandle;
	
	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> CurrentPhaseAbilityHandles;
};
