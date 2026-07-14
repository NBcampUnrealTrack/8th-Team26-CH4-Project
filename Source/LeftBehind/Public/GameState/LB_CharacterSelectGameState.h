// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "System/CharacterSelect/LBCharacterSelectTypes.h"
#include "LB_CharacterSelectGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FLBCharacterSelectSnapshotChanged,
	const FLBCharacterSelectSnapshot&,
	Snapshot);

// 캐릭터 선택 화면의 공용 상태 복제

UCLASS()
class LEFTBEHIND_API ALB_CharacterSelectGameState : public AGameStateBase
{
	GENERATED_BODY()
	
public:

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	const FLBCharacterSelectSnapshot& GetSnapshot() const { return Snapshot; }

	void SetSnapshot_ServerOnly(const FLBCharacterSelectSnapshot& NewSnapshot);

	UPROPERTY(BlueprintAssignable)
	FLBCharacterSelectSnapshotChanged OnSnapshotChanged;

protected:

	UPROPERTY(ReplicatedUsing=OnRep_Snapshot)
	FLBCharacterSelectSnapshot Snapshot;

	UFUNCTION()
	void OnRep_Snapshot();
};
