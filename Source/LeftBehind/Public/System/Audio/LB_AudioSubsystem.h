// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LB_AudioSubsystem.generated.h"

class UAudioComponent;
class USoundBase;

UCLASS()
class LEFTBEHIND_API ULB_AudioSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:

	// 이미 같은 BGM이 재생 중이면 아무 작업도 하지 않음
	UFUNCTION(BlueprintCallable, Category="LB|Audio")
	void PlayBGM(
		USoundBase* NewBGM,
		float VolumeMultiplier = 1.f,
		float PitchMultiplier = 1.f);

	// 현재 BGM 정지
	UFUNCTION(BlueprintCallable, Category="LB|Audio")
	void StopBGM(float FadeOutTime = 0.f);

	// 현재 BGM Fade Out
	UFUNCTION(BlueprintCallable, Category="LB|Audio")
	void FadeOutBGM(float FadeOutTime);

	// 새로운 BGM Fade In
	UFUNCTION(BlueprintCallable, Category="LB|Audio")
	void FadeInBGM(
		USoundBase* NewBGM,
		float FadeInTime,
		float TargetVolume = 1.f);

	// 현재 재생 중인지
	UFUNCTION(BlueprintPure, Category="LB|Audio")
	bool IsPlayingBGM(USoundBase* Sound) const;

private:

	// 현재 재생 중인 AudioComponent
	UPROPERTY()
	TObjectPtr<UAudioComponent> BGMComponent;

	// 현재 재생 중인 Sound
	UPROPERTY()
	TObjectPtr<USoundBase> CurrentBGM;
};
