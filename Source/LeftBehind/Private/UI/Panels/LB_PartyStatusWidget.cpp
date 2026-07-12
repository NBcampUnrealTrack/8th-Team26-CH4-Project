// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Panels/LB_PartyStatusWidget.h"
#include "Components/VerticalBox.h"
#include "UI/Panels/LB_PartyMemberSlotWidget.h"
#include "GameState/LB_RaidGameState.h"
#include "Player/LB_PlayerState.h"

void ULB_PartyStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindRaidGameState();
	RefreshPartyMembers();
}

void ULB_PartyStatusWidget::NativeDestruct()
{
	UnbindRaidGameState();

	Super::NativeDestruct();
}

void ULB_PartyStatusWidget::BindRaidGameState()
{
	UnbindRaidGameState();

	if (UWorld* World = GetWorld())
	{
		CachedRaidGameState = Cast<ALB_RaidGameState>(World->GetGameState());
	}

	if (CachedRaidGameState)
	{
		PlayerArrayChangedHandle = CachedRaidGameState->OnRaidPlayerArrayChanged.AddUObject(
			this,
			&ThisClass::RefreshPartyMembers);
	}
}

void ULB_PartyStatusWidget::UnbindRaidGameState()
{
	if (CachedRaidGameState && PlayerArrayChangedHandle.IsValid())
	{
		CachedRaidGameState->OnRaidPlayerArrayChanged.Remove(PlayerArrayChangedHandle);
	}

	PlayerArrayChangedHandle.Reset();
	CachedRaidGameState = nullptr;
}

void ULB_PartyStatusWidget::RefreshPartyMembers()
{
	if (!PartyMemberContainer) return;
	
	PartyMemberContainer->ClearChildren();
	
	ALB_RaidGameState* RaidGameState = CachedRaidGameState;
	if (!RaidGameState && GetWorld())
	{
		RaidGameState = Cast<ALB_RaidGameState>(GetWorld()->GetGameState());
	}
	if (!RaidGameState) return;
	
	TArray<ALB_PlayerState*> CurrentRaidPlayerStates;
	RaidGameState->GetCurrentRaidPlayerStates(CurrentRaidPlayerStates);

	for (ALB_PlayerState* LBPS : CurrentRaidPlayerStates)
	{
		ULB_PartyMemberSlotWidget* MemberSlot = CreateWidget<ULB_PartyMemberSlotWidget>(GetOwningPlayer(), PartyMemberSlotClass);
		if (!MemberSlot) continue;
		
		MemberSlot->SetPlayerState(LBPS);
		
		PartyMemberContainer->AddChild(MemberSlot);
	}
}
