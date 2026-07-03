// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Characters/LB_BaseCharacter.h"
#include "LB_BossCharacter.generated.h"


class ULB_AbilitySystemComponent;
//페이지 교체 시의 정보, 처음 페이즈는 생략할 것
USTRUCT()
struct FPhaseInfo
{
	GENERATED_BODY()
	
	FGameplayTag PhaseTag;
	float HealthThreshold;
	
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPhaseChange,FGameplayTag,PhaseTag);

UCLASS()
class LEFTBEHIND_API ALB_BossCharacter : public ALB_BaseCharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ALB_BossCharacter();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	FPhaseChange PhaseChange;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated)
	bool bIsBeingLaunched{false};
	
	void StopMovementUntilLanded();
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual UAttributeSet* GetAttributeSet() const override;
	virtual void HandleDeath() override;
	
	virtual void HandlePaseChanged(const FOnAttributeChangeData& AttributeChangeData);
	
	virtual int32 CalculatePhase(const FOnAttributeChangeData& AttributeChangeData);

	
private:
	
	UFUNCTION()
	void EnableMovementOnLanded(const FHitResult& Hit);
	
	UPROPERTY()
	TObjectPtr<ULB_AbilitySystemComponent> AbilitySystemComponent;
	
	UPROPERTY()
	TObjectPtr<UAttributeSet> Attributeset;
	
	UPROPERTY(EditAnywhere, Category = "Boss|Phase", meta=(AllowPrivateAccess=true))
	TArray<FPhaseInfo> PhaseInfos;
	
	UPROPERTY()
	int32 CurrentPhaseIndex = 0;
	

};
