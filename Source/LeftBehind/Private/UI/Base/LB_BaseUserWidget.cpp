// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Base/LB_BaseUserWidget.h"

void ULB_BaseUserWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	CachePlayerController();
	CachePawn();

	InitializeWidget();
}

void ULB_BaseUserWidget::NativeDestruct()
{
	ReleaseWidget();
	Super::NativeDestruct();
}

void ULB_BaseUserWidget::InitializeWidget()
{
}

void ULB_BaseUserWidget::ReleaseWidget()
{
}

APlayerController* ULB_BaseUserWidget::GetLBPlayerController() const
{
	return CachedPlayerController.Get();
}

APawn* ULB_BaseUserWidget::GetLBPawn() const
{
	return CachedPawn.Get();
}

void ULB_BaseUserWidget::CachePlayerController()
{
	CachedPlayerController = GetOwningPlayer();
}

void ULB_BaseUserWidget::CachePawn()
{
	if (!CachedPlayerController.IsValid())
	{
		CachePlayerController();
	}
	CachedPawn = CachedPlayerController.IsValid()
		? CachedPlayerController->GetPawn()
		: nullptr;
}
