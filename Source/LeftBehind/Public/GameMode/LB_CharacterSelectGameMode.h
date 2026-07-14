// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LB_CharacterSelectGameMode.generated.h"

// 캐릭터 선택 화면에서 모든 Preview Actor를 생성하고 초기화하는 GameMode

class UDataTable;
class ALB_CharacterPreview;
struct FLBCharacterData;
class ATargetPoint;
class ULB_CharacterSelectWidget;

UCLASS()
class LEFTBEHIND_API ALB_CharacterSelectGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:

	virtual void BeginPlay() override;
	
private:

	void SetupInput();
	
	void GetTargetPoints();
	
	// Preview Actor 생성
	void InitCharacterPreview();

	// DT Row 이름 순서대로 반환
	void GetCharacterRows(TArray<FName>& OutRows) const;

	void CreateCharacterSelectWidget();
	
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
};
