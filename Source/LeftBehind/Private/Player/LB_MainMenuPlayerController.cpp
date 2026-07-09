// LB_MainMenuPlayerController.cpp

#include "Player/LB_MainMenuPlayerController.h"

ALB_MainMenuPlayerController::ALB_MainMenuPlayerController()
{
	bShowMouseCursor = true;
}

void ALB_MainMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	bShowMouseCursor = true;

	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}

void ALB_MainMenuPlayerController::RequestStartHunt()
{
	if (HasAuthority())
	{
		ServerStartHunt();
		return;
	}


	ServerStartHunt();
}

void ALB_MainMenuPlayerController::ServerStartHunt_Implementation()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 서버가 맵을 이동하면 Listen Server에 붙은 클라이언트들도 같이 따라간다.
	World->ServerTravel(HuntMapURL);
}
