// LB_PrimaryAttackAbility.h

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/LB_GameplayAbility.h"
#include "LB_PrimaryAttackAbility.generated.h"

class UAnimMontage;
class UGameplayEffect;

UCLASS()
class LEFTBEHIND_API ULB_PrimaryAttackAbility : public ULB_GameplayAbility
{
	GENERATED_BODY()

public:
	ULB_PrimaryAttackAbility();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;

protected:
	// 실제 HP를 줄이는 GameplayEffect다. 에디터에서는 GE_Damage를 넣으면 된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Attack")
	TSubclassOf<UGameplayEffect> DamageEffect;

	// 첫 번째 클릭에 재생할 기본 공격 몽타주다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Animation")
	TObjectPtr<UAnimMontage> PrimaryMontageA;

	// 두 번째 클릭에 재생할 기본 공격 몽타주다. 이후 A/B가 반복된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Animation")
	TObjectPtr<UAnimMontage> PrimaryMontageB;

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

	// 바닥보다 조금 위에서 판정하도록 높이를 올려 보스 캡슐과 안정적으로 겹치게 한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Attack")
	float HitBoxElevationOffset = 80.f;

	// true면 공격 범위는 빨간 구체, 맞은 대상은 초록 구체로 보여 디버깅하기 쉽다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Debug")
	bool bDrawHitDebug = false;

private:
	// InstancedPerActor Ability라서 플레이어마다 A/B 순서를 따로 기억한다.
	UPROPERTY(Transient)
	int32 NextMontageIndex = 0;

	UAnimMontage* SelectNextPrimaryMontage();
	void PlayPrimaryMontage(AActor* AvatarActor);
};
