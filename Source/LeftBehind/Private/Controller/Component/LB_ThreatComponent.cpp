// Fill out your copyright notice in the Description page of Project Settings.


#include "Controller/Component/LB_ThreatComponent.h"

#include "AbilitySystem/LB_AttributeSet.h"


// Sets default values for this component's properties
ULB_ThreatComponent::ULB_ThreatComponent()
{

	PrimaryComponentTick.bCanEverTick = false;


}

void ULB_ThreatComponent::BeginPlay()
{
	Super::BeginPlay();
	
	ALB_BaseCharacter* Boss = Cast<ALB_BaseCharacter>(GetOwner());
	if (!IsValid(Boss)) return;
	
	ULB_AttributeSet* BossAttributeSet = Cast<ULB_AttributeSet>(Boss->GetAttributeSet()) ;
	if (!IsValid(BossAttributeSet)) return;
	
	BossAttributeSet->ActorDamaged.AddDynamic(this,&ULB_ThreatComponent::UpdateDamageMap);
	
}

void ULB_ThreatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	
	ALB_BaseCharacter* Boss = Cast<ALB_BaseCharacter>(GetOwner());
	if (!IsValid(Boss)) return;
	
	ULB_AttributeSet* BossAttributeSet = Cast<ULB_AttributeSet>(Boss->GetAttributeSet()) ;
	if (!IsValid(BossAttributeSet)) return;
	
	BossAttributeSet->ActorDamaged.RemoveDynamic(this,&ULB_ThreatComponent::UpdateDamageMap);
}

AActor* ULB_ThreatComponent::SelectMostThreatCharacter()
{
	ALB_BaseCharacter* CurrentTarget = Cast<ALB_BaseCharacter>(Target);
	if (!Target) return nullptr;
	
	float CurrentTargetThreat = ThreatMap.FindRef(CurrentTarget);
	
	ALB_BaseCharacter* NewTarget = nullptr;
	float NewTargetThreat = 0;
	
	for (const TPair<ALB_BaseCharacter*, float>& Pair : ThreatMap)
	{
		if (NewTargetThreat < Pair.Value)
		{
			NewTarget = Pair.Key;
			NewTargetThreat = Pair.Value;
		}
	}
	
	if (NewTarget == nullptr || NewTarget == Target) return Target;
	if (CurrentTargetThreat* Margin >= NewTargetThreat) return Target;
	
	
	Target = NewTarget;
	OnThreatTargetChanged.Broadcast(Target);
	
	
	return Target;
	
	
}

void ULB_ThreatComponent::UpdateThreatMap( AActor* Instigator,  AActor* Causer, float Damage)
{
	//지금은 데미지 지표만 넣어놓고, 추후 힐량 등등의 요소 추가
	float Threat = Damage;
	if (ALB_BaseCharacter* ALB_Instigator = Cast<ALB_BaseCharacter>(Instigator))
	{
		ThreatMap.FindOrAdd(ALB_Instigator)+=Threat;	
	}
	

	
	
	
}

void ULB_ThreatComponent::UpdateDamageMap( AActor* Instigator,  AActor* Causer, float Damage)
{
	if (ALB_BaseCharacter* ALB_Instigator = Cast<ALB_BaseCharacter>(Instigator))
	{
		DamageMap.FindOrAdd(ALB_Instigator)+=Damage;	
	}
	
	
	UpdateThreatMap( Instigator, Causer, Damage);
	
}




