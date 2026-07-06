// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayTagContainer.h"
#include "Characters/LB_BaseCharacter.h"
#include "GameplayTags/LBTags.h"
#include "LB_BossCharacter.generated.h"

struct FGameplayAbilitySpecHandle;
class ULB_AttackPatternComponent;
class ULB_ThreatComponent;
class UAttributeSet;
class ULB_AbilitySystemComponent;

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
	virtual UAttributeSet* GetAttributeSet() const override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;



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

private:
	UFUNCTION()
	void EnableMovementOnLanded(const FHitResult& Hit);
	
	UPROPERTY()
	TObjectPtr<ULB_AbilitySystemComponent> AbilitySystemComponent;
	
	UPROPERTY()
	TObjectPtr<UAttributeSet> Attributeset;
	
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
