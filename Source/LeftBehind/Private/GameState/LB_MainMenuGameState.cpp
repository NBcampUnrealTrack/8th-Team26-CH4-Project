#include "GameState/LB_MainMenuGameState.h"

#include "Net/UnrealNetwork.h"

ALB_MainMenuGameState::ALB_MainMenuGameState()
{
	bReplicates = true;
	SetNetUpdateFrequency(10.f);
	SetMinNetUpdateFrequency(2.f);
}

void ALB_MainMenuGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(
		ALB_MainMenuGameState,
		MainMenuSnapshot,
		COND_None,
		REPNOTIFY_Always);
}

void ALB_MainMenuGameState::SetMainMenuSnapshot_ServerOnly(const FLBMainMenuSnapshot& NewSnapshot)
{
	if (!HasAuthority() || MainMenuSnapshot == NewSnapshot)
	{
		return;
	}

	MainMenuSnapshot = NewSnapshot;
	OnRep_MainMenuSnapshot();
	ForceNetUpdate();
}

void ALB_MainMenuGameState::OnRep_MainMenuSnapshot()
{
	OnMainMenuSnapshotChanged.Broadcast(MainMenuSnapshot);
}
