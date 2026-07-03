//LBRaidBossBase.h
//테스트 보스 추가

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "LBRaidBossBase.generated.h"

struct FOnAttributeChangeData;
class UAbilitySystemComponent;
class UAttributeSet;
class ULB_AbilitySystemComponent;
class ULB_AttributeSet;

// 보스 HP가 바뀔 때 GameMode/GameState/UI에 현재 HP와 최대 HP를 전달한다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLBBossHPChangedSignature, float, CurrentHP, float, MaxHP);
// 보스가 사망했을 때 레이드 종료 처리를 시작하기 위한 이벤트.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLBBossDiedSignature);

// 레이드 보스 공통 베이스 클래스. HP/방어력/사망 상태를 서버 권한으로 관리하고 복제한다.
UCLASS()
class LEFTBEHIND_API ALBRaidBossBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ALBRaidBossBase();

	virtual void BeginPlay() override;
	// CurrentHP, MaxHP, DEF, bIsDead를 클라이언트에 복제 대상으로 등록한다.
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// 보스의 AttributeSet을 읽는다. UI나 블루프린트가 GAS 수치를 직접 볼 때 사용한다.
	UFUNCTION(BlueprintPure, Category="LB|Boss|GAS")
	UAttributeSet* GetAttributeSet() const;

	UFUNCTION(BlueprintPure, Category="LB|Boss|GAS")
	ULB_AttributeSet* GetLBAttributeSet() const;

	// HP 변경을 외부 로직과 UI에 알리는 블루프린트 바인딩 이벤트.
	UPROPERTY(BlueprintAssignable, Category="LB|Boss")
	FOnLBBossHPChangedSignature OnBossHPChanged;

	// 사망 시 GameMode가 승리 처리 등을 수행하도록 알리는 이벤트.
	UPROPERTY(BlueprintAssignable, Category="LB|Boss")
	FOnLBBossDiedSignature OnBossDied;

	// 데이터 테이블에서 읽은 MaxHP/DEF로 보스 스탯을 서버에서 초기화한다.
	UFUNCTION(BlueprintCallable, Category="LB|Boss")
	void InitializeBossStats_ServerOnly(float InMaxHP, float InDEF);

	// 서버 권한으로 데미지를 적용하고 HP가 0 이하가 되면 사망 처리한다.
	UFUNCTION(BlueprintCallable, Category="LB|Boss")
	void ApplyRaidDamage_ServerOnly(float DamageAmount);

	// 현재 HP를 읽는다.
	UFUNCTION(BlueprintPure, Category="LB|Boss")
	float GetCurrentHP() const { return CurrentHP; }

	// 최대 HP를 읽는다.
	UFUNCTION(BlueprintPure, Category="LB|Boss")
	float GetMaxHP() const { return MaxHP; }

	// 방어력 값을 읽는다.
	UFUNCTION(BlueprintPure, Category="LB|Boss")
	float GetDEF() const { return DEF; }

	// 보스 사망 여부를 읽는다.
	UFUNCTION(BlueprintPure, Category="LB|Boss")
	bool IsDead() const { return bIsDead; }

protected:
	// 레이드 보스도 ASC를 가진다. 데미지/버프/디버프는 이 통로를 통해 서버에서 계산된다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="LB|Boss|GAS")
	TObjectPtr<ULB_AbilitySystemComponent> AbilitySystemComponent;

	// Health/MaxHealth를 보관하는 GAS AttributeSet이다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="LB|Boss|GAS")
	TObjectPtr<ULB_AttributeSet> AttributeSet;

	// 현재 체력. RepNotify로 HP 변경 이벤트를 클라이언트에서 발생시킨다.
	UPROPERTY(ReplicatedUsing=OnRep_CurrentHP, BlueprintReadOnly, Category="LB|Boss")
	float CurrentHP = 10000.f;

	// 최대 체력. 초기화 후 GameState의 보스 HP 표시 기준이 된다.
	UPROPERTY(Replicated, BlueprintReadOnly, Category="LB|Boss")
	float MaxHP = 10000.f;

	// 방어력. 확장된 데미지 공식이나 UI 표시에서 사용할 수 있다.
	UPROPERTY(Replicated, BlueprintReadOnly, Category="LB|Boss")
	float DEF = 50.f;

	// 이미 사망 처리된 보스에 데미지/사망 이벤트가 중복 적용되지 않도록 막는다.
	UPROPERTY(Replicated, BlueprintReadOnly, Category="LB|Boss")
	bool bIsDead = false;

	// CurrentHP가 복제될 때 HP 변경 델리게이트를 방송한다.
	UFUNCTION()
	void OnRep_CurrentHP();

	void BindGASAttributeDelegates();
	void HandleHealthAttributeChanged(const FOnAttributeChangeData& AttributeChangeData);
	void HandleMaxHealthAttributeChanged(const FOnAttributeChangeData& AttributeChangeData);
	// 서버에서 보스 사망 상태를 확정하고 사망 이벤트를 한 번만 방송한다.
	void Die_ServerOnly();
};
