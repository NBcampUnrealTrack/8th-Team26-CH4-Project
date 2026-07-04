// Fill out your copyright notice in the Description page of Project Settings.


#include "Controller/Component/LB_ThreatComponent.h"

#include "AbilitySystem/LB_AttributeSet.h"
#include "Characters/LB_BaseCharacter.h"


// Sets default values for this component's properties
ULB_ThreatComponent::ULB_ThreatComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
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

ALB_BaseCharacter* ULB_ThreatComponent::SelectMostThreatCharacter()
{
	Target = UpdateThreatMap();
	
	if (ALB_BaseCharacter* ALB_Target = Cast<ALB_BaseCharacter>(Target))
		return ALB_Target;
	
	return nullptr;
	
	
}

AActor* ULB_ThreatComponent::UpdateThreatMap()
{
	AActor* MostThreater = nullptr;
	float StrongestDamage = 0;
	
	for (const TPair<AActor*, float>& Pair : DamageMap)
	{
		if (StrongestDamage < Pair.Value)
		{
			MostThreater = Pair.Key;
			StrongestDamage = Pair.Value;
		}
	}
	
	if (MostThreater == nullptr || MostThreater == Target) return nullptr;
	
	return MostThreater;
	
	
	
}

void ULB_ThreatComponent::UpdateDamageMap( AActor* Instigator,  AActor* Causer, float Damage)
{
	if (ALB_BaseCharacter* ALB_Instigator = Cast<ALB_BaseCharacter>(Instigator))
	{
		DamageMap.FindOrAdd(ALB_Instigator)+=Damage;	
	}
	
	
	UpdateThreatMap();
	
}




