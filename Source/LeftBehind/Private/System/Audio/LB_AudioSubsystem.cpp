// Fill out your copyright notice in the Description page of Project Settings.


#include "System/Audio/LB_AudioSubsystem.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

void ULB_AudioSubsystem::PlayBGM(USoundBase* NewBGM, float VolumeMultiplier, float PitchMultiplier)
{
	if (!NewBGM)
	{
		return;
	}

	// 같은 음악이 이미 재생 중이면 아무것도 하지 않는다.
	if (CurrentBGM == NewBGM &&
		BGMComponent &&
		BGMComponent->IsPlaying())
	{
		return;
	}

	if (BGMComponent)
	{
		BGMComponent->Stop();
		BGMComponent->DestroyComponent();
		BGMComponent = nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	BGMComponent = UGameplayStatics::SpawnSound2D(
		World,
		NewBGM,
		VolumeMultiplier,
		PitchMultiplier,
		0.f,
		nullptr,
		true,   // Persist Across Level Transition
		true);  // Auto Destroy

	CurrentBGM = NewBGM;
}

void ULB_AudioSubsystem::StopBGM(float FadeOutTime)
{
	if (!BGMComponent)
	{
		return;
	}

	if (FadeOutTime > 0.f)
	{
		BGMComponent->FadeOut(FadeOutTime, 0.f);
	}
	else
	{
		BGMComponent->Stop();
	}

	BGMComponent = nullptr;
	CurrentBGM = nullptr;
}

void ULB_AudioSubsystem::FadeOutBGM(float FadeOutTime)
{
	if (!BGMComponent)
	{
		return;
	}

	BGMComponent->FadeOut(FadeOutTime, 0.f);

	BGMComponent = nullptr;
	CurrentBGM = nullptr;
}

void ULB_AudioSubsystem::FadeInBGM(USoundBase* NewBGM, float FadeInTime, float TargetVolume)
{
	if (!NewBGM)
	{
		return;
	}

	if (CurrentBGM == NewBGM &&
		BGMComponent &&
		BGMComponent->IsPlaying())
	{
		return;
	}

	if (BGMComponent)
	{
		BGMComponent->Stop();
		BGMComponent->DestroyComponent();
		BGMComponent = nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	BGMComponent = UGameplayStatics::SpawnSound2D(
		World,
		NewBGM,
		0.f,
		1.f,
		0.f,
		nullptr,
		true,
		false);

	if (BGMComponent)
	{
		BGMComponent->FadeIn(FadeInTime, TargetVolume);
	}

	CurrentBGM = NewBGM;
}

bool ULB_AudioSubsystem::IsPlayingBGM(USoundBase* Sound) const
{
	return
		BGMComponent &&
		BGMComponent->IsPlaying() &&
		CurrentBGM == Sound;
}
