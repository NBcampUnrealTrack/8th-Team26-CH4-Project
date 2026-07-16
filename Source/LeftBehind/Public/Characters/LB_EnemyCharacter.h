// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LB_BaseCharacter.h"
#include "LB_EnemyCharacter.generated.h"

class ULB_ThreatComponent;
class ULB_AbilitySystemComponent;
class UAttributeSet;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLB_MinionsDiedSignature,ALB_EnemyCharacter*, DeathMinion);

UCLASS()
class LEFTBEHIND_API ALB_EnemyCharacter : public ALB_BaseCharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ALB_EnemyCharacter();
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	
	void StopMovementUntilLanded();

	virtual void BeginPlay() override;
	
	virtual void Tick(float DeltaSeconds) override;
	
	UFUNCTION()
	void Die_ServerOnly();
	
	UFUNCTION()
	void OnRep_CurrentHP();
	
	UPROPERTY(BlueprintAssignable, Category="LB|Enemy")
	FOnLB_MinionsDiedSignature OnMinionsDied;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated)
	bool bIsBeingLaunched{false};
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly, Category="LB|Enemy")
	bool bIsMinions = false;
	
protected:
	// Called when the game starts or when spawned

	virtual UAttributeSet* GetAttributeSet() const override;
	virtual void HandleDeath() override;
	
	//Status
	UPROPERTY(ReplicatedUsing=OnRep_CurrentHP, BlueprintReadOnly, Category="LB|Enemy")
	float CurrentHP = 10000.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="LB|Enemy")
	float MaxHP = 10000.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="LB|Enemy")
	float DEF = 50.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="LB|Enemy")
	bool bIsDead = false;

	
private:
	
	UFUNCTION()
	void EnableMovementOnLanded(const FHitResult& Hit);
	
	UPROPERTY()
	TObjectPtr<ULB_AbilitySystemComponent> AbilitySystemComponent;
	
	UPROPERTY()
	TObjectPtr<UAttributeSet> Attributeset;
	

public:
};
