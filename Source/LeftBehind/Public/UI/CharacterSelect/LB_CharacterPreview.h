// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LB_CharacterPreview.generated.h"

struct FLBCharacterData;
class USceneCaptureComponent2D;
class UMeshComponent;

UCLASS()
class LEFTBEHIND_API ALB_CharacterPreview : public AActor
{
	GENERATED_BODY()

public:

	ALB_CharacterPreview();
	
	UFUNCTION()
	void Initialize(const FLBCharacterData& Data);
	
	FORCEINLINE USkeletalMeshComponent* GetMesh() const { return SkeletalMeshComp; }

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LB|Preview")
	TObjectPtr<UMaterialInstance> RenderMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LB|Preview")
	TObjectPtr<UTextureRenderTarget2D> RenderTexture;
	
protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<USkeletalMeshComponent> SkeletalMeshComp;
	
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneCaptureComponent2D> SceneCapture;
};
