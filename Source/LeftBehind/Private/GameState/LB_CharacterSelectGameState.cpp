// Fill out your copyright notice in the Description page of Project Settings.


#include "GameState/LB_CharacterSelectGameState.h"
#include "Net/UnrealNetwork.h"

void ALB_CharacterSelectGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ALB_CharacterSelectGameState, Snapshot);
}

void ALB_CharacterSelectGameState::SetSnapshot_ServerOnly(const FLBCharacterSelectSnapshot& NewSnapshot)
{
	if (!HasAuthority())
	{
		return;
	}

	Snapshot = NewSnapshot;

	OnRep_Snapshot();

	ForceNetUpdate();
}

void ALB_CharacterSelectGameState::OnRep_Snapshot()
{
	OnSnapshotChanged.Broadcast(Snapshot);
}
