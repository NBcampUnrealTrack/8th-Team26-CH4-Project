// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Characters/LB_BaseCharacter.h"
#include "Camera/CameraComponent.h"
#include "AbilitySystemInterface.h"   
#include "LB_PlayerCharacter.generated.h"
class USpringArmComponent;
class UCameraComponent;
class UAbilitySystemComponent;
class UAttributeSet;

UCLASS()
class LEFTBEHIND_API ALB_PlayerCharacter : public ALB_BaseCharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ALB_PlayerCharacter();
	virtual void BeginPlay() override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual UAttributeSet* GetAttributeSet() const override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

protected:
	virtual void HandleDeath() override;

private:
	void InitializeAbilityActorInfo();
	void BindHealthChangedDelegate();


public:
	
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;
	
	
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;
};
