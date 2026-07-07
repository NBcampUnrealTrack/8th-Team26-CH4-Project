// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LB_Projectile.generated.h"


class UProjectileMovementComponent;
class UGameplayEffect;

UCLASS()
class LEFTBEHIND_API ALB_Projectile : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ALB_Projectile();
	virtual void NotifyActorBeginOverlap(AActor* OtherActor);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LB|Heal", meta = (ExposeOnSpawn, ClampMin = "0.0"))
	float Heal{10.f};

protected:
	UPROPERTY(VisibleAnywhere, Category = "LB|Projectile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;
	
	UPROPERTY(EditAnywhere, Category = "LB|Heal")
	TSubclassOf<UGameplayEffect> DamageEffect;
};
