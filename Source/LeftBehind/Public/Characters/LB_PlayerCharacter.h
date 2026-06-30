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
UCLASS()
class LEFTBEHIND_API ALB_PlayerCharacter : public ALB_BaseCharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ALB_PlayerCharacter();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void PossessedBy(AController* NewController) override;


public:
	
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;
	
	
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;
};
