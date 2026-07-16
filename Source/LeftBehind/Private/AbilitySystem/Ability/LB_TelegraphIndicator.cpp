// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Ability/LB_TelegraphIndicator.h"

#include "Components/DecalComponent.h"


ALB_TelegraphIndicator::ALB_TelegraphIndicator()
{
	bReplicates = true;
	Decal = CreateDefaultSubobject<UDecalComponent>("Decal");
	RootComponent = Decal;
	Decal->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
}

void ALB_TelegraphIndicator::SetAsCircle(float Radius)
{
	Decal->SetDecalMaterial(CircleMaterial); // 원형 텍스처 머티리얼
	Decal->DecalSize = FVector(64.f, Radius, Radius);
}

void ALB_TelegraphIndicator::SetAsLine(float Width, float Length)
{
	Decal->SetDecalMaterial(LineMaterial); // 화살표/라인 텍스처 머티리얼
	Decal->DecalSize = FVector(64.f, Width, Length);
}



