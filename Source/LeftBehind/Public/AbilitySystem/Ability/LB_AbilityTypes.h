// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LB_AbilityTypes.generated.h"

class ALB_EnemyCharacter;
class UGameplayEffect;
/**
 * 
 */
USTRUCT(BlueprintType)
struct FLB_AttackConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Attack")
	TSubclassOf<UGameplayEffect> DamageEffect = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Attack")
	float Damage = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Attack")
	float HitBoxRadius = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Attack")
	float HitBoxForwardOffset = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Attack")
	float HitBoxElevationOffset = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Attack")
	bool bDrawHitDebug = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Attack")
	float KnockbackForce = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Attack")
	float KnockbackForceV = 0.f;
};

USTRUCT(BlueprintType)
struct FLB_SummonSpawnParams
{
	GENERATED_BODY()

	//소환하려는 소환수 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Attack")
	TArray<TSubclassOf<ALB_EnemyCharacter>> MinionClass;

	//스폰 횟수
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Attack")
	int32 SpawnCount = 1;

	//최소 스폰 거리
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Attack")
	float MinSpawnRadius = 200.f;

	//최대 스폰 거리
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Attack")
	float MaxSpawnRadius = 600.f;

	//
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Attack")
	TArray<TSubclassOf<UGameplayEffect>> InitialEffects;

	//최대로 소환 가능한 소환 수 개수
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Attack")
	int32 MaxActiveMinions = 3;
};
