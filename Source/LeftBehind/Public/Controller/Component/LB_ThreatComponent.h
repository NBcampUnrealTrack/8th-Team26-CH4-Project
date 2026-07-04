// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LB_ThreatComponent.generated.h"


class ALB_BaseCharacter;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LEFTBEHIND_API ULB_ThreatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	ULB_ThreatComponent();
	
	virtual void BeginPlay() override;
	
	ALB_BaseCharacter* SelectMostThreatCharacter();
	
	
private:
	AActor* UpdateThreatMap();
	
	UFUNCTION()
	void UpdateDamageMap( AActor* Instigator,  AActor* Causer, float Damage);
	
	
	TMap<ALB_BaseCharacter*, float> ThreatMap;
	TMap<ALB_BaseCharacter*, float> DamageMap;
	
	AActor* Target;
	
	
};
