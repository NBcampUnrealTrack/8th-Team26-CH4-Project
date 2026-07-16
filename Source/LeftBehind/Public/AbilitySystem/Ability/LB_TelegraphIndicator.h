// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LB_TelegraphIndicator.generated.h"

UCLASS()
class LEFTBEHIND_API ALB_TelegraphIndicator : public AActor
{
	GENERATED_BODY()

public:
	ALB_TelegraphIndicator();

	UPROPERTY(VisibleAnywhere)
	UDecalComponent* Decal;

	// 원형 범위 표시 (소환, ShockWave 등)
	UFUNCTION(BlueprintCallable)
	void SetAsCircle(float Radius);

	// 직선 라인 표시 (돌진 등)
	UFUNCTION(BlueprintCallable)
	void SetAsLine(float Width, float Length);
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities|Telegraph")
	TObjectPtr<UMaterial> CircleMaterial;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities|Telegraph")
	TObjectPtr<UMaterial> LineMaterial;
	
	
};
