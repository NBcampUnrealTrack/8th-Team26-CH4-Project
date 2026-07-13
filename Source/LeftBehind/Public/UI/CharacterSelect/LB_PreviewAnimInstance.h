// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "LB_PreviewAnimInstance.generated.h"

// 캐릭터 선택창 3D 프리뷰용 AnimInstance 부모 클래스

UCLASS(Blueprintable)
class LEFTBEHIND_API ULB_PreviewAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
	
public:

	// 기본 보기 / 상세 보기 상태
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LB|Preview")
	bool bDetailView = false;

	// UI에서 상세 보기 전환
	UFUNCTION(BlueprintCallable, Category="LB|Preview")
	void SetDetailView(bool bInDetailView);
};
