// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "LB_BaseCharacter.generated.h"


class UGameplayAbility;
class UGameplayEffect;
class UAttributeSet;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FASCInitialized, UAbilitySystemComponent*, ASC, UAttributeSet*, AS);

UCLASS(Abstract)
class LEFTBEHIND_API ALB_BaseCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ALB_BaseCharacter();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual UAttributeSet* GetAttributeSet() const;
	
	UPROPERTY(BlueprintAssignable)
	FASCInitialized OnAscInitialized;
	
protected:
	void GiveStartupAbilities();
	void InitializeAttribute() const;
	
private:
	
	UPROPERTY(EditDefaultsOnly, Category= "LeftBehind|Abilities")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;
	
	UPROPERTY(EditDefaultsOnly, Category = "LeftBehind|Effects")
	TSubclassOf<UGameplayEffect> InitializeAttributesEffect;


	
};
