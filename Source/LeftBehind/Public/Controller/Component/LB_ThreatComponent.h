// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Characters/LB_BaseCharacter.h"
#include "Components/ActorComponent.h"
#include "LB_ThreatComponent.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnThreatTargetChanged, AActor*, NewTarget);



UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LEFTBEHIND_API ULB_ThreatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	ULB_ThreatComponent();
	FOnThreatTargetChanged OnThreatTargetChanged;
	
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	//노릴 최우선 목표 최신화 및 반환
	AActor* SelectMostThreatCharacter();
	
	
private:
	//위협도 맵 업데이트
	void UpdateThreatMap( AActor* Instigator,  AActor* Causer, float Damage);
	
	//데미지 지표 업데이트
	UFUNCTION()
	void UpdateDamageMap( AActor* Instigator,  AActor* Causer, float Damage);
	
	//어그로가 근소한 차이로 연속적으로 바뀔 경우를 고려 
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="AI|Margin",meta=(AllowPrivateAccess = true))
	float Margin = 1.2f;
	
	TMap<ALB_BaseCharacter*, float> ThreatMap;
	TMap<ALB_BaseCharacter*, float> DamageMap;
	
	//Behavior Tree와의 연계를 위해서 필요.
	AActor* Target;
	
	
};
