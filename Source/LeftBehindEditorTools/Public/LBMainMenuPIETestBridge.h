#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LBMainMenuPIETestBridge.generated.h"

class ALB_MainMenuPlayerController;
class UUserWidget;

/** Editor-only bridge that escapes the Python ProcessEvent stack before invoking an owning-client action. */
UCLASS()
class ULBMainMenuPIETestBridge : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="LB|Tests")
	static bool ScheduleCodenameSubmit(ALB_MainMenuPlayerController* Controller, const FString& Codename);

	UFUNCTION(BlueprintCallable, Category="LB|Tests")
	static bool ScheduleStartRequests(ALB_MainMenuPlayerController* Controller, int32 RequestCount = 1);

	UFUNCTION(BlueprintCallable, Category="LB|Tests")
	static bool ScheduleLobbyReady(ALB_MainMenuPlayerController* Controller, bool bReady = true);

	UFUNCTION(BlueprintCallable, Category="LB|Tests")
	static bool ScheduleWidgetClick(UUserWidget* Widget, FName ButtonWidgetName);

	UFUNCTION(BlueprintCallable, Category="LB|Tests")
	static bool ScheduleCodenameWidgetSubmit(UUserWidget* Widget, const FString& Codename);
};
