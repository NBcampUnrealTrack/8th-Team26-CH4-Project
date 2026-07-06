// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LB_BaseUserWidget.generated.h"

// 모든 프로젝트 UI의 공통 부모 클래스
// PlayerController와 Pawn을 캐싱하고 공통 초기화/해제

UCLASS(Abstract)
class LEFTBEHIND_API ULB_BaseUserWidget : public UUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 위젯 생성 후 공통 초기화가 끝나면 호출
	virtual void InitializeWidget();
	// 위젯 제거 직전에 호출
	virtual void ReleaseWidget();

public:
	UFUNCTION(BlueprintPure, Category="LB|UI")
	APlayerController* GetLBPlayerController() const;

	UFUNCTION(BlueprintPure, Category="LB|UI")
	APawn* GetLBPawn() const;

protected:
	void CachePlayerController();
	void CachePawn();

protected:
	UPROPERTY(BlueprintReadOnly, Category="LB|UI")
	TWeakObjectPtr<APlayerController> CachedPlayerController;

	UPROPERTY(BlueprintReadOnly, Category="LB|UI")
	TWeakObjectPtr<APawn> CachedPawn;
};
