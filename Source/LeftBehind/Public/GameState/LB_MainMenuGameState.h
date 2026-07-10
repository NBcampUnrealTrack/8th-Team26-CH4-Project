#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "System/MainMenu/LBMainMenuTypes.h"
#include "LB_MainMenuGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnLBMainMenuSnapshotChanged,
	const FLBMainMenuSnapshot&,
	Snapshot);

/** 모든 메뉴 클라이언트가 공유해야 하는 작은 로비 스냅샷만 복제한다. */
UCLASS()
class LEFTBEHIND_API ALB_MainMenuGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ALB_MainMenuGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintAssignable, Category="LB|MainMenu")
	FOnLBMainMenuSnapshotChanged OnMainMenuSnapshotChanged;

	UFUNCTION(BlueprintPure, Category="LB|MainMenu")
	const FLBMainMenuSnapshot& GetMainMenuSnapshot() const { return MainMenuSnapshot; }

	UFUNCTION(BlueprintPure, Category="LB|MainMenu")
	bool IsLobbyReady() const { return MainMenuSnapshot.Phase == ELBMainMenuPhase::Ready; }

	void SetMainMenuSnapshot_ServerOnly(const FLBMainMenuSnapshot& NewSnapshot);

protected:
	UPROPERTY(ReplicatedUsing=OnRep_MainMenuSnapshot, BlueprintReadOnly, Category="LB|MainMenu")
	FLBMainMenuSnapshot MainMenuSnapshot;

	UFUNCTION()
	void OnRep_MainMenuSnapshot();
};
