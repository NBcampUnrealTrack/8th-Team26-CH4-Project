// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "LB_BaseCharacter.generated.h"


class ULB_AttackPatternComponent;
class ULB_ThreatComponent;
struct FOnAttributeChangeData;
class UGameplayAbility;
class UGameplayEffect;
class UAttributeSet;
class UAnimMontage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FASCInitialized, UAbilitySystemComponent*, ASC, UAttributeSet*, AS);

UCLASS(Abstract)
class LEFTBEHIND_API ALB_BaseCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ALB_BaseCharacter();
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual UAttributeSet* GetAttributeSet() const;

	// 서버가 호출하면 모든 클라이언트가 같은 몽타주를 재생한다. 공격 판정은 서버, 모션은 모두에게 보이는 연출이다.
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayCosmeticMontage(UAnimMontage* Montage, float PlayRate = 1.f);
	
	//서버 호출 시에 클라이언트의 애니메이션 동작을 멈춘다.
	UFUNCTION(NetMulticast, Unreliable, BlueprintCallable, Category = "LeftBehind|Animation")
	void MulticastFreezePose();
	
	UPROPERTY(BlueprintAssignable)
	FASCInitialized OnAscInitialized;
	
	bool IsAlive() const { return bAlive; }
	
	// ---- Attack ----
	ULB_ThreatComponent* GetThreatComponent() {return ThreatComponent;}
	ULB_AttackPatternComponent* GetAttackComponent() { return AttackPatternComponent;}
	
	UPROPERTY(EditAnywhere, Category = "Crash|AI")
	float SearchRange{1000.f};
	
	//Player에게 접근하기 위한 거리
	UPROPERTY(EditAnywhere,BlueprintReadOnly, Category = "Enemy|AI")
	float MeleeDistance;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly, Category = "Enemy|AI")
	float RangedDistance;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly, Category = "Enemy|AI")
	float WideAttackTrigger;
	
protected:
	void GiveStartupAbilities();
	void InitializeAttribute() const;
	
	void OnHealthChanged(const FOnAttributeChangeData& AttributeChangeData);
	virtual void HandleDeath();
	virtual void HandleRespon();
	
	
	// --- AI ---
	TObjectPtr<ULB_ThreatComponent> ThreatComponent;
	
	TObjectPtr<ULB_AttackPatternComponent> AttackPatternComponent;
	

	
private:

	UPROPERTY(EditDefaultsOnly, Category= "LeftBehind|Abilities")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	UPROPERTY(EditDefaultsOnly, Category = "LeftBehind|Effects")
	TSubclassOf<UGameplayEffect> InitializeAttributesEffect;

	// 사망 시 재생할 몽타주다. 캐릭터마다 다르면 이 값만 에디터에서 교체하면 된다.
	UPROPERTY(EditDefaultsOnly, Category = "LeftBehind|Animation")
	TObjectPtr<UAnimMontage> DeathMontage;

	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"), Replicated)
	bool bAlive = true;

};
