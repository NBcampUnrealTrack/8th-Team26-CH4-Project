// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Ability/LB_AbilityTypes.h"
#include "LB_BossChargeAbility.generated.h"


/**
 * 
 */
UCLASS()
class LEFTBEHIND_API ULB_BossChargeAbility : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	ULB_BossChargeAbility();
	
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle, 
		const FGameplayAbilityActorInfo* ActorInfo, 
		const FGameplayAbilityActivationInfo ActivationInfo, 
		const FGameplayEventData* TriggerEventData) override;
	
	//전조 행동 Task가 끝난 이후에 작동시키기 위한 델리게이트 함수
	UFUNCTION()
	virtual void OnAbilityActivated();
	
	virtual void HandleActivateAbility(		
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
		);
	
	//Charge Task가 끝난 이후에 작동시키기 위한 델리게이트 함순
	UFUNCTION()
	virtual void OnChargeCompleted();
	
	//전조 행동 Task가 비정상적으로 종료될 때 작동시키기 위한 델리게이트 함수
	UFUNCTION()
	virtual void OnAbilityCancelled();
	
	//Charge Task를 작동시키기 위한 함수
	void StartCharge();
	
	virtual void PlayMontage(AActor* AvatarActor);
	
	//Task 생성을 위해서 필요한 Montage
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Attack")
	TObjectPtr<UAnimMontage> TelegraphMontage;
	
	//GA에서 재생된 Montage
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Attack")
	TObjectPtr<UAnimMontage> PrimaryMontage;
	
	//돌진 거리
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="LB|Attack")
	float ChargingDistance;
	
	//넉백 시 밀려나는 수직 파워
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="LB|Attack")
	float ChargingSpeed;
	
	//넉백 시 밀려나는 파워
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="LB|Attack")
	float KnockbackForce;
	
	//넉백 시 밀려나는 수직 파워
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="LB|Attack")
	float KnockbackForceV;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Attack")
	TSubclassOf<UGameplayEffect> DamageEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Attack")
	bool bIsTelegraph;
	
	// 공격 몽타주 재생 속도다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Animation", meta = (ClampMin = "0.01"))
	float MontagePlayRate = 1.f;

	// 입력 한 번당 줄일 HP다. 이 값은 GE_Damage의 SetByCaller 값으로 서버에서만 적용된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Attack", meta = (ClampMin = "0.0"))
	float Damage = 20.f;

	// 캐릭터 앞에 만드는 공격 판정 구체의 반지름이다. 보스처럼 큰 대상은 넉넉한 값이 테스트에 좋다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Attack", meta = (ClampMin = "1.0"))
	float HitBoxRadius = 300.f;

	// 캐릭터 위치에서 정면으로 얼마나 떨어진 곳에 판정 구체를 만들지 정한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Attack")
	float HitBoxForwardOffset = 170.f;

	// 바닥보다 조금 위에서 판정하도록 높이를 올려 캐릭터 캡슐과 안정적으로 겹치게 한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Attack")
	float HitBoxElevationOffset = 80.f;

	// true면 공격 범위는 빨간 구체, 맞은 대상은 초록 구체로 보여 디버깅하기 쉽다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Debug")
	bool bDrawHitDebug = false;
	
private:
	FLB_AttackConfig AttackConfig;
	
	
};
