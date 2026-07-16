// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "System/CharacterSelect/LBCharacterSelectTypes.h"
#include "LB_CharacterSelectGameMode.generated.h"

// 캐릭터 선택 화면에서 모든 Preview Actor를 생성하고 초기화하는 GameMode

class UDataTable;
class ALB_CharacterPreview;
struct FLBCharacterData;
class ATargetPoint;
class ULB_CharacterSelectWidget;
class ALB_CharacterSelectGameState;
class ALB_MainMenuPlayerController;
class ALB_PlayerState;

UCLASS()
class LEFTBEHIND_API ALB_CharacterSelectGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	ALB_CharacterSelectGameMode();
	virtual void BeginPlay() override;
	
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	
	ELBCharacterSelectResult TrySelectCharacter(ALB_MainMenuPlayerController* RequestingController, ELBCharacterID CharacterID);

	bool TrySetCharacterReady(ALB_MainMenuPlayerController* RequestingController);

	bool TryCancelCharacterReady(ALB_MainMenuPlayerController* RequestingController);
	
	bool IsCharacterAlreadySelected(ELBCharacterID CharacterID, const ALB_PlayerState* IgnorePlayer) const;
	
	
	UFUNCTION(BlueprintPure, Category="LB|MainMenu|Travel")
	TSoftObjectPtr<UWorld> GetRaidMap() const { return RaidMap; }
	
	
	bool CanStartRaid(const APlayerController* RequestingController) const;

	bool TryStartRaid(APlayerController* RequestingController);


protected:
	// World soft reference이므로 Class Defaults에 맵 에셋 선택기가 나타나며 cooker도 의존성을 추적할 수 있다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="LB|MainMenu|Travel")
	TSoftObjectPtr<UWorld> RaidMap;

	
private:

	void GetTargetPoints();
	void InitCharacterPreview();
	void GetCharacterRows(TArray<FName>& OutRows) const;
	
	bool GetRaidMapPackageName(FString& OutPackageName) const;
	bool StartRaidTravel();
	
private:
	UPROPERTY()
	TArray<TObjectPtr<ATargetPoint>> TargetPoints;
	
	// 캐릭터 데이터 테이블
	UPROPERTY(EditDefaultsOnly, Category="LB|Character")
	TObjectPtr<UDataTable> CharacterDataTable;

	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<ULB_CharacterSelectWidget> CharacterSelectWidgetClass;
	
	// Preview Actor BP
	UPROPERTY(EditDefaultsOnly, Category="LB|Character")
	TSubclassOf<ALB_CharacterPreview> CharacterPreviewClass;

	// 생성된 Preview Actor
	UPROPERTY()
	TArray<TObjectPtr<ALB_CharacterPreview>> CharacterPreviews;
	
	void RefreshSnapshot();

	FLBCharacterSelectSnapshot BuildSnapshot(bool bAdvanceRevision);

	ALB_CharacterSelectGameState* GetCharacterSelectGameState() const;

	int32 SnapshotRevision = 0;
	
	bool AreAllPlayersReady() const;
};
