// Fill out your copyright notice in the Description page of Project Settings.


#include "LeftBehind/Public/Characters/LB_BaseCharacter.h"


// Sets default values
ALB_BaseCharacter::ALB_BaseCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}

UAbilitySystemComponent* ALB_BaseCharacter::GetAbilitySystemComponent() const
{
	return nullptr;
}



