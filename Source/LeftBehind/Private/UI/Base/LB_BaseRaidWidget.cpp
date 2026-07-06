// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Base/LB_BaseRaidWidget.h"
#include "GameState/LB_RaidGameState.h"



void ULB_BaseRaidWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	BindRaidGameState();
	SyncCurrentRaidState();
}

void ULB_BaseRaidWidget::NativeDestruct()
{
	UnbindRaidGameState();
	
	Super::NativeDestruct();
}

ALB_RaidGameState* ULB_BaseRaidWidget::GetRaidGameState() const
{
	return CachedRaidGameState;
}

void ULB_BaseRaidWidget::OnRaidStateChanged(ELBRaidState NewState)
{
	HandleRaidStateChanged(NewState);
	BP_OnRaidStateChanged(NewState);
}

void ULB_BaseRaidWidget::OnBossHPChanged(float CurrentHP, float MaxHP)
{
	HandleBossHPChanged(CurrentHP, MaxHP);
	BP_OnBossHPChanged(CurrentHP, MaxHP);
}

void ULB_BaseRaidWidget::OnRaidResultChanged(const FLBRaidResultData& ResultData)
{
	HandleRaidResultChanged(ResultData);
	BP_OnRaidResultChanged(ResultData);
}

void ULB_BaseRaidWidget::HandleRaidStateChanged(ELBRaidState NewState)
{
}

void ULB_BaseRaidWidget::HandleBossHPChanged(float CurrentHP, float MaxHP)
{
}

void ULB_BaseRaidWidget::HandleRaidResultChanged(const FLBRaidResultData& ResultData)
{
}

void ULB_BaseRaidWidget::BP_OnRaidStateChanged_Implementation(ELBRaidState NewState)
{
}

void ULB_BaseRaidWidget::BP_OnBossHPChanged_Implementation(float CurrentHP, float MaxHP)
{
}

void ULB_BaseRaidWidget::BP_OnRaidResultChanged_Implementation(const FLBRaidResultData& ResultData)
{
}

void ULB_BaseRaidWidget::BindRaidGameState()
{
	if (CachedRaidGameState) return;
	if (UWorld* World = GetWorld())
	{
		CachedRaidGameState = World->GetGameState<ALB_RaidGameState>();
	}
	
	if (!CachedRaidGameState) return;
	
	CachedRaidGameState->OnRaidStateChanged.AddUniqueDynamic(
		this,
		&ThisClass::OnRaidStateChanged);

	CachedRaidGameState->OnBossHPChanged.AddUniqueDynamic(
		this,
		&ThisClass::OnBossHPChanged);

	CachedRaidGameState->OnRaidResultChanged.AddUniqueDynamic(
		this,
		&ThisClass::OnRaidResultChanged);
}

void ULB_BaseRaidWidget::UnbindRaidGameState()
{
	if (!CachedRaidGameState) return;
	
	CachedRaidGameState->OnRaidStateChanged.RemoveDynamic(
		this,
		&ThisClass::OnRaidStateChanged);

	CachedRaidGameState->OnBossHPChanged.RemoveDynamic(
		this,
		&ThisClass::OnBossHPChanged);

	CachedRaidGameState->OnRaidResultChanged.RemoveDynamic(
		this,
		&ThisClass::OnRaidResultChanged);

	CachedRaidGameState = nullptr;
}

void ULB_BaseRaidWidget::SyncCurrentRaidState()
{
	if (!CachedRaidGameState) return;
	
	OnRaidStateChanged(CachedRaidGameState->RaidState);

	OnBossHPChanged(
		CachedRaidGameState->BossCurrentHP,
		CachedRaidGameState->BossMaxHP);

	OnRaidResultChanged(
		CachedRaidGameState->RaidResult);
}
