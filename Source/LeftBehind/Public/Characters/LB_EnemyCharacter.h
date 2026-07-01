// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LB_BaseCharacter.h"
#include "LB_EnemyCharacter.generated.h"

class ULB_AbilitySystemComponent;
class UAttributeSet;

UCLASS()
class LEFTBEHIND_API ALB_EnemyCharacter : public ALB_BaseCharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ALB_EnemyCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual UAttributeSet* GetAttributeSet() const override;
	
private:
	
	UPROPERTY()
	TObjectPtr<ULB_AbilitySystemComponent> AbilitySystemComponent;
	
	UPROPERTY()
	TObjectPtr<UAttributeSet> Attributeset;
	

public:
};
