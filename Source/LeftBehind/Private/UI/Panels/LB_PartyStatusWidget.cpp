// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Panels/LB_PartyStatusWidget.h"
#include "Components/VerticalBox.h"
#include "UI/Panels/LB_PartyMemberSlotWidget.h"
#include "GameFramework/GameStateBase.h"
#include "Player/LB_PlayerState.h"

void ULB_PartyStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	RefreshPartyMembers();
}

void ULB_PartyStatusWidget::RefreshPartyMembers()
{
	if (!PartyMemberContainer) return;
	
	PartyMemberContainer->ClearChildren();
	
	AGameStateBase* GS = GetWorld()->GetGameState();
	if (!GS) return;
	
	for (APlayerState* PS : GS->PlayerArray)
	{
		ALB_PlayerState* LBPS = Cast<ALB_PlayerState>(PS);
		if (!LBPS) continue;
		
		ULB_PartyMemberSlotWidget* MemberSlot = CreateWidget<ULB_PartyMemberSlotWidget>(GetOwningPlayer(), PartyMemberSlotClass);
		if (!MemberSlot) continue;
		
		MemberSlot->SetPlayerState(LBPS);
		
		PartyMemberContainer->AddChild(MemberSlot);
	}
}
