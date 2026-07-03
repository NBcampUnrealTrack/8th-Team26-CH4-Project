// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/LB_GameplayAbility.h"
#include "LB_HitReact.generated.h"

/**
 * 
 */
UCLASS()
class LEFTBEHIND_API ULB_HitReact : public ULB_GameplayAbility
{
	GENERATED_BODY()
	
public:

	UFUNCTION(BlueprintCallable, Category = "Crash|Abilities")
	void CacheHitDirectionVectors(AActor* Instigator);

	UPROPERTY(BlueprintReadOnly, Category = "Crash|Abilities")
	FVector AvatarForward;

	UPROPERTY(BlueprintReadOnly, Category = "Crash|Abilities")
	FVector ToInstigator;
};
