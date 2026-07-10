#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "System/MainMenu/LBMainMenuTypes.h"
#include "LB_MainMenuGameMode.generated.h"

class ALB_MainMenuGameState;
class ALB_MainMenuPlayerController;
class UWorld;

/**
 * Listen Server 메뉴의 참가자 정책과 ServerTravel만 담당한다.
 * 로컬 UI는 각 ALB_MainMenuPlayerController가 소유한다.
 */
UCLASS()
class LEFTBEHIND_API ALB_MainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ALB_MainMenuGameMode();

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	UFUNCTION(BlueprintPure, Category="LB|MainMenu|Travel")
	bool CanStartHunt(const APlayerController* RequestingController) const;

	bool TryStartHunt(APlayerController* RequestingController);
	ELBCodenameSubmitResult TryConfirmCodename(
		ALB_MainMenuPlayerController* RequestingController,
		const FString& RawCodename,
		FString& OutSanitizedCodename);

	static ELBCodenameSubmitResult ValidateCodename(
		const FString& RawCodename,
		FString& OutSanitizedCodename);

	UFUNCTION(BlueprintPure, Category="LB|MainMenu|Travel")
	int32 GetMinPlayersToStart() const { return FMath::Max(1, MinPlayersToStart); }

	UFUNCTION(BlueprintPure, Category="LB|MainMenu|Travel")
	TSoftObjectPtr<UWorld> GetRaidMap() const { return RaidMap; }

	bool StartsPlayersWithoutMenuPawns() const { return bStartPlayersAsSpectators; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="LB|MainMenu|Travel", meta=(ClampMin="1"))
	int32 MinPlayersToStart = 2;

	// World soft reference이므로 Class Defaults에 맵 에셋 선택기가 나타나며 cooker도 의존성을 추적할 수 있다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="LB|MainMenu|Travel")
	TSoftObjectPtr<UWorld> RaidMap;

private:
	UPROPERTY(Transient)
	bool bTravelInProgress = false;

	int32 SnapshotRevision = 0;

	void RefreshLobbySnapshot();
	FLBMainMenuSnapshot BuildLobbySnapshot(bool bAdvanceRevision);
	bool AreAllActivePlayersLoaded() const;
	bool GetRaidMapPackageName(FString& OutPackageName) const;
	ALB_MainMenuGameState* GetMainMenuGameState() const;
};
