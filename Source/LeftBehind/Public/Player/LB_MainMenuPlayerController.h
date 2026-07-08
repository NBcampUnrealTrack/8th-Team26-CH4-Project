// LB_MainMenuPlayerController.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LB_MainMenuPlayerController.generated.h"

UCLASS()
class LEFTBEHIND_API ALB_MainMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ALB_MainMenuPlayerController();

protected:
	virtual void BeginPlay() override;

public:
	// WBP_MainMenu의 사냥시작 버튼이 호출할 함수다.
	UFUNCTION(BlueprintCallable, Category = "LB|MainMenu")
	void RequestStartHunt();

private:
	// 실제 맵 이동은 서버에서만 해야 멀티플레이 흐름이 안정적
	UFUNCTION(Server, Reliable)
	void ServerStartHunt();

	UPROPERTY(EditDefaultsOnly, Category = "LB|MainMenu")
	FString HuntMapURL = TEXT("/Game/LeftBehind/Maps/Blockout/Main?listen");
};