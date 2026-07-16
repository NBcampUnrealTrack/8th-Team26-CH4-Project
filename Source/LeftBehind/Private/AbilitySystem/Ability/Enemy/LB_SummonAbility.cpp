// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Ability/Enemy/LB_SummonAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Ability/LB_TelegraphIndicator.h"
#include "AbilitySystem/Task/LB_TelegraphAbilityTask.h"
#include "Characters/LB_EnemyCharacter.h"
#include "Characters/Boss/LB_BossCharacter.h"
#include "GameplayTags/LBTags.h"
#include "Utils/LB_BlueprintLibrary.h"

ULB_SummonAbility::ULB_SummonAbility()
{
	// 중요 로직은 서버만 수행한다. 클라이언트에서도 몬스터가 스폰될 경우 문제가 생긴다.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void ULB_SummonAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	if (!AvatarActor->HasAuthority())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	ALB_BossCharacter* BossCharacter = Cast<ALB_BossCharacter>(AvatarActor);
	if (!BossCharacter)
	{
		ClearIndicator();
		EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo()	,GetCurrentActivationInfo()	,true,true);
		return;
	}
	
	PendingSpawnLocations.Empty();
	SpawnedIndicators.Empty();

	
	
	TArray<ALB_EnemyCharacter*> ActiveMinions = BossCharacter->GetActiveMinions();
	const int32 NeedToSpawn = SummonSpawnParams.SpawnCount - ActiveMinions.Num();
	if (NeedToSpawn == 0)
	{
		EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo()	,GetCurrentActivationInfo()	,true,true);
		return;
	}
	
	for (int32 i =0; i < NeedToSpawn; ++i)
	{
		const FVector SpawnLocation = ULB_BlueprintLibrary::GetRandomSpawnLocation(
			AvatarActor, SummonSpawnParams.MinSpawnRadius, SummonSpawnParams.MaxSpawnRadius);
		PendingSpawnLocations.Add(SpawnLocation);

		if (IndicatorClass)
		{
			FActorSpawnParameters IndicatorSpawnParams;
			if (ALB_TelegraphIndicator* Indicator = GetWorld()->SpawnActor<ALB_TelegraphIndicator>(
				IndicatorClass, SpawnLocation, FRotator::ZeroRotator, IndicatorSpawnParams))
			{
				Indicator->SetAsCircle(IndicatorRadius);
				SpawnedIndicators.Add(Indicator);
			}
		}
	}
	
	if (bIsTelegraph)
	{
		ULB_TelegraphAbilityTask* TelegraphAbilityTask = 
			ULB_TelegraphAbilityTask::PlayTelegraph(
				this,
				TelegraphMontage,
				LBTags::LBAbilities::Enemy::Telegraph,
				LBTags::LBCues::Enemy::TelegraphCue);
		
		TelegraphAbilityTask->OnTaskCompleted.AddDynamic(this,&ULB_SummonAbility::OnAbilityActivated);
		TelegraphAbilityTask->OnTaskCancelled.AddDynamic(this, &ULB_SummonAbility::OnAbilityCancelled);
		TelegraphAbilityTask->ReadyForActivation();
	}
	else
	{
		
		OnAbilityActivated();
	}
	
	
	
}

void ULB_SummonAbility::OnAbilityActivated()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
    if (!AvatarActor)
    {
    	ClearIndicator();
    	EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo()	,GetCurrentActivationInfo()	,true,true);
        return;
    }

    if (SummonSpawnParams.MinionClass.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[%s] MinionClass가 비어있습니다."), *GetName());
    	
    	ClearIndicator();
       	EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo()	,GetCurrentActivationInfo()	,true,true);
        return;
    }
	
	ALB_BossCharacter* BossCharacter = Cast<ALB_BossCharacter>(AvatarActor);
	if (!BossCharacter)
	{
		ClearIndicator();
		EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo()	,GetCurrentActivationInfo()	,true,true);
		return;
	}
	
    UWorld* World = AvatarActor->GetWorld();
    UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	TArray<ALB_EnemyCharacter*> ActiveMinions = BossCharacter->GetActiveMinions();
    int32 SpawnedCount = 0;

    for (int32 i = 0; i < SummonSpawnParams.SpawnCount; ++i)
    {
        // 최대 소환 수 도달 시 중단
        if (ActiveMinions.Num() >= SummonSpawnParams.MaxActiveMinions)
        {
            UE_LOG(LogTemp, Log, TEXT("[%s] 최대 소환수(%d) 도달, 스폰 중단"),
                *GetName(), SummonSpawnParams.MaxActiveMinions);
            break;
        }

        // 스폰할 클래스 랜덤 선택
        const int32 RandomIndex = FMath::RandRange(0, SummonSpawnParams.MinionClass.Num() - 1);
        TSubclassOf<AActor> ClassToSpawn = SummonSpawnParams.MinionClass[RandomIndex];
        if (!ClassToSpawn)
        {
            continue;
        }
		//스폰 위치를 가늠하기 위한 랜덤 좌표 스폰
		
        const FRotator SpawnRotation = AvatarActor->GetActorRotation();

    	
    	if (!PendingSpawnLocations.IsValidIndex(i))
    	{
    		UE_LOG(LogTemp, Error,
				TEXT("Invalid PendingSpawnLocations index %d / %d"),
				i,
				PendingSpawnLocations.Num());

    		break;
    	}
    	
    	FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = AvatarActor;
        SpawnParams.Instigator = Cast<APawn>(AvatarActor);
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        ALB_EnemyCharacter* SpawnedMinion = World->SpawnActor<ALB_EnemyCharacter>(
            ClassToSpawn, PendingSpawnLocations[i], SpawnRotation, SpawnParams);

        if (!SpawnedMinion)
        {
            UE_LOG(LogTemp, Warning, TEXT("[%s] Summon Failed: %s"), *GetName(), *ClassToSpawn->GetName());
            continue;
        }

        // 초기 GameplayEffect 적용
        if (UAbilitySystemComponent* MinionASC = SpawnedMinion->GetAbilitySystemComponent())
        {
            for (const TSubclassOf<UGameplayEffect>& EffectClass : SummonSpawnParams.InitialEffects)
            {
                if (!EffectClass)
                {
                    continue;
                }

                FGameplayEffectContextHandle ContextHandle = SourceASC->MakeEffectContext();
                ContextHandle.AddSourceObject(AvatarActor);

                FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(
                    EffectClass, GetAbilityLevel(), ContextHandle);

                if (SpecHandle.IsValid())
                {
                    SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), MinionASC);
                }
            }
        }

        BossCharacter->AddActiveMinions(SpawnedMinion);
        ++SpawnedCount;
    }
	
	ClearIndicator();

    UE_LOG(LogTemp, Log, TEXT("[%s]  %d summon success (Current Active minions: %d/%d)"),
        *GetName(), SpawnedCount, ActiveMinions.Num(), SummonSpawnParams.MaxActiveMinions);
	
	
	EndAbility(GetCurrentAbilitySpecHandle()
	,GetCurrentActorInfo()
	,GetCurrentActivationInfo()
	,true
	,false);
}

void ULB_SummonAbility::OnAbilityCancelled()
{
	ClearIndicator();
	
	EndAbility(GetCurrentAbilitySpecHandle()
	,GetCurrentActorInfo()
	,GetCurrentActivationInfo()
	,true
	,true);
}

void ULB_SummonAbility::ClearIndicator()
{

	for (ALB_TelegraphIndicator* Indicator : SpawnedIndicators)
	{
		if (IsValid(Indicator))
		{
			Indicator->Destroy();
		}
	}
	
	PendingSpawnLocations.Empty();
	SpawnedIndicators.Empty();
}


