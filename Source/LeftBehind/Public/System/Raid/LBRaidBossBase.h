//LBRaidBossBase.h
//테스트 보스 추가

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "LBRaidBossBase.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLBBossHPChangedSignature, float, CurrentHP, float, MaxHP);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLBBossDiedSignature);

UCLASS()
class LEFTBEHIND_API ALBRaidBossBase : public ACharacter
{
	GENERATED_BODY()

public:
	ALBRaidBossBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintAssignable, Category="LB|Boss")
	FOnLBBossHPChangedSignature OnBossHPChanged;

	UPROPERTY(BlueprintAssignable, Category="LB|Boss")
	FOnLBBossDiedSignature OnBossDied;

	UFUNCTION(BlueprintCallable, Category="LB|Boss")
	void InitializeBossStats_ServerOnly(float InMaxHP, float InDEF);

	UFUNCTION(BlueprintCallable, Category="LB|Boss")
	void ApplyRaidDamage_ServerOnly(float DamageAmount);

	UFUNCTION(BlueprintPure, Category="LB|Boss")
	float GetCurrentHP() const { return CurrentHP; }

	UFUNCTION(BlueprintPure, Category="LB|Boss")
	float GetMaxHP() const { return MaxHP; }

	UFUNCTION(BlueprintPure, Category="LB|Boss")
	float GetDEF() const { return DEF; }

	UFUNCTION(BlueprintPure, Category="LB|Boss")
	bool IsDead() const { return bIsDead; }

protected:
	UPROPERTY(ReplicatedUsing=OnRep_CurrentHP, BlueprintReadOnly, Category="LB|Boss")
	float CurrentHP = 10000.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="LB|Boss")
	float MaxHP = 10000.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="LB|Boss")
	float DEF = 50.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="LB|Boss")
	bool bIsDead = false;

	UFUNCTION()
	void OnRep_CurrentHP();

	void Die_ServerOnly();
};