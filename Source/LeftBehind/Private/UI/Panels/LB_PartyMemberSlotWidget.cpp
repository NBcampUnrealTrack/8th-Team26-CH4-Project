// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Panels/LB_PartyMemberSlotWidget.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/LB_AttributeSet.h"
#include "AbilitySystem/LB_AbilitySystemComponent.h"
#include "Player/LB_PlayerState.h"
#include "Engine/DataTable.h"

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
	OnCharacterIDChanged(CachedPlayerState->GetCharacterID());
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

	CachedPlayerState->OnCharacterIDChanged.AddDynamic(
		this,
		&ThisClass::OnCharacterIDChanged);
	
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

	CachedPlayerState->OnCharacterIDChanged.RemoveDynamic(
		this,
		&ThisClass::OnCharacterIDChanged);
	
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

	MaxHealthChangedHandle =
		CachedASC->GetGameplayAttributeValueChangeDelegate(
			ULB_AttributeSet::GetMaxHealthAttribute())
		.AddUObject(
			this,
			&ThisClass::OnMaxHealthChanged);

	CachedAttributeSet->OnAttributesInitialized.AddUniqueDynamic(
		this,
		&ThisClass::OnAttributesInitialized);
}

void ULB_PartyMemberSlotWidget::UnbindAttributes()
{
	if (CachedASC)
	{
		if (HealthChangedHandle.IsValid())
		{
			CachedASC->GetGameplayAttributeValueChangeDelegate(
				ULB_AttributeSet::GetHealthAttribute())
				.Remove(HealthChangedHandle);
		}

		if (MaxHealthChangedHandle.IsValid())
		{
			CachedASC->GetGameplayAttributeValueChangeDelegate(
				ULB_AttributeSet::GetMaxHealthAttribute())
				.Remove(MaxHealthChangedHandle);
		}
	}

	if (CachedAttributeSet)
	{
		CachedAttributeSet->OnAttributesInitialized.RemoveDynamic(
			this,
			&ThisClass::OnAttributesInitialized);
	}
	
	HealthChangedHandle.Reset();
	MaxHealthChangedHandle.Reset();

	CachedASC = nullptr;
	CachedAttributeSet = nullptr;
}

void ULB_PartyMemberSlotWidget::OnRoleChanged(ELBRoleType NewRoleType)
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

void ULB_PartyMemberSlotWidget::OnMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	RefreshAll();
}

void ULB_PartyMemberSlotWidget::OnAttributesInitialized()
{
	RefreshAll();
}

void ULB_PartyMemberSlotWidget::RefreshAll()
{
	if (!CachedPlayerState || !CachedAttributeSet) return;
	if (!CachedAttributeSet->bAttributeInitialized || CachedAttributeSet->GetMaxHealth() <= 0.f) return;
	
	BP_UpdatePartyMember(
		CachedPlayerState->GetPlayerNameText(),
		CachedPlayerState->GetRoleType(),
		CachedAttributeSet->GetHealth(),
		CachedAttributeSet->GetMaxHealth(),
		CachedPlayerState->IsDead()
		);
}

void ULB_PartyMemberSlotWidget::OnCharacterIDChanged(ELBCharacterID NewCharacterID)
{
	if (!CharacterDataTable)
	{
		return;
	}
	
	const FString EnumName = StaticEnum<ELBCharacterID>()->GetNameStringByValue((int64)NewCharacterID);
	const FLBCharacterData* CharacterData = CharacterDataTable->FindRow<FLBCharacterData>(FName(*EnumName), TEXT("Party"));
	
	if (!CharacterData) return;
	
	UE_LOG(LogTemp, Warning, TEXT("ULB_PartyMemberSlotWidget::OnCharacterIDChanged : %s"),
		*GetNameSafe(CharacterData->HUDPortraitImage));
	
	BP_UpdatePortrait(CharacterData->HUDPortraitImage);
}
