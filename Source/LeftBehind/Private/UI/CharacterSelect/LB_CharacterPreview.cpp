// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/CharacterSelect/LB_CharacterPreview.h"
#include "System/Character/LBCharacterTypes.h"

ALB_CharacterPreview::ALB_CharacterPreview()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	SkeletalMeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMesh"));

	SkeletalMeshComp->SetupAttachment(RootComponent);
}

void ALB_CharacterPreview::Initialize(const FLBCharacterData& Data)
{
	SkeletalMeshComp->SetSkeletalMeshAsset(Data.PreviewMesh);
	SkeletalMeshComp->SetAnimInstanceClass(Data.PreviewAnimClass);
	SkeletalMeshComp->SetRelativeLocation(Data.MeshOffset);
	SkeletalMeshComp->SetRelativeRotation(Data.MeshRotation);
	SkeletalMeshComp->SetRelativeScale3D(Data.MeshScale);

	RenderMaterial = Data.PreviewMaterialInstance;
	RenderTexture = Data.PreviewRenderTarget;

	SetupRenderTarget();
}
