// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Panels/LB_PartyMemberSlotWidget.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/LB_AttributeSet.h"
#include "AbilitySystem/LB_AbilitySystemComponent.h"
#include "Player/LB_PlayerState.h"

void ULB_PartyMemberSlotWidget::SetPlayerState(ALB_PlayerState* InPlayerState)
{
	if (!InPlayerState) return;
	if (CachedPlayerState == InPlayerState) return;
	
	UnbindAttributes();
	UnbindPlayerState();
	
	CachedPlayerState = InPlayerState;
	
	BindPlayerState();
	BindAttributes();

	RefreshAll();
}

void ULB_PartyMemberSlotWidget::NativeDestruct()
{
	UnbindAttributes();
	UnbindPlayerState();
	
	Super::NativeDestruct();
}

void ULB_PartyMemberSlotWidget::BindPlayerState()
{
	if (!CachedPlayerState) return;
	
	CachedPlayerState->OnRoleChanged.AddDynamic(
		this,
		&ThisClass::OnRoleChanged);

	CachedPlayerState->OnDeadStateChanged.AddDynamic(
		this,
		&ThisClass::OnDeadStateChanged);
}

void ULB_PartyMemberSlotWidget::UnbindPlayerState()
{
	if (!CachedPlayerState) return;
	
	CachedPlayerState->OnRoleChanged.RemoveDynamic(
		this,
		&ThisClass::OnRoleChanged);

	CachedPlayerState->OnDeadStateChanged.RemoveDynamic(
		this,
		&ThisClass::OnDeadStateChanged);

	CachedPlayerState = nullptr;
}

void ULB_PartyMemberSlotWidget::BindAttributes()
{
	if (!CachedPlayerState) return;
	
	CachedASC = CachedPlayerState->GetLBAbilitySystemComponent();
	CachedAttributeSet = CachedPlayerState->GetLBAttributeSet();
	
	if (!CachedASC || !CachedAttributeSet) return;
	
	HealthChangedHandle =
		CachedASC->GetGameplayAttributeValueChangeDelegate(
			ULB_AttributeSet::GetHealthAttribute())
		.AddUObject(
			this,
			&ThisClass::OnHealthChanged);
}

void ULB_PartyMemberSlotWidget::UnbindAttributes()
{
	if (!CachedASC || !HealthChangedHandle.IsValid()) return;
	
	CachedASC->GetGameplayAttributeValueChangeDelegate(
			ULB_AttributeSet::GetHealthAttribute())
			.Remove(HealthChangedHandle);
	
	HealthChangedHandle.Reset();

	CachedASC = nullptr;
	CachedAttributeSet = nullptr;
}

void ULB_PartyMemberSlotWidget::OnRoleChanged(FName NewRoleID)
{
	RefreshAll();
}

void ULB_PartyMemberSlotWidget::OnDeadStateChanged(bool bIsDead)
{
	RefreshAll();
}

void ULB_PartyMemberSlotWidget::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	RefreshAll();
}

void ULB_PartyMemberSlotWidget::RefreshAll()
{
	if (!CachedPlayerState || !CachedAttributeSet) return;
	
	BP_UpdatePartyMember(
		CachedPlayerState->GetPlayerNameText(),
		CachedPlayerState->GetRoleID(),
		CachedAttributeSet->GetHealth(),
		CachedAttributeSet->GetMaxHealth(),
		CachedPlayerState->IsDead()
		);
}
