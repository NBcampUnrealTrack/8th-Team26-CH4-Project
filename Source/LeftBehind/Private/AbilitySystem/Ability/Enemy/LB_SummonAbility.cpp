// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Ability/Enemy/LB_SummonAbility.h"

#include "AbilitySystem/Task/LB_TelegraphAbilityTask.h"
#include "Characters/LB_EnemyCharacter.h"
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
	
	
	
}

void ULB_SummonAbility::OnAbilityActivated()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
    if (!AvatarActor)
    {
    	EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo()	,GetCurrentActivationInfo()	,true,true);
        return;
    }

    if (SummonSpawnParams.MinionClass.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[%s] MinionClass가 비어있습니다."), *GetName());
       	EndAbility(GetCurrentAbilitySpecHandle(),GetCurrentActorInfo()	,GetCurrentActivationInfo()	,true,true);
        return;
    }

    // 죽거나 소멸한 소환수 정리 (스폰 전에 먼저 갱신)
    /*ActiveMinions.RemoveAll([](const TWeakObjectPtr<ALB_EnemyCharacter>& Minion)
    {
        return !Minion.IsValid();
    });*/

    UWorld* World = AvatarActor->GetWorld();
    UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();

    int32 SpawnedCount = 0;

    /*for (int32 i = 0; i < SummonSpawnParams.SpawnCount; ++i)
    {*/
        // 최대 소환 수 도달 시 중단
        /*if (ActiveMinions.Num() >= SummonSpawnParams.MaxActiveMinions)
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

        const FVector SpawnLocation = ULB_BlueprintLibrary::GetRandomSpawnLocation(
            AvatarActor, SummonSpawnParams.MinSpawnRadius, SummonSpawnParams.MaxSpawnRadius);
        const FRotator SpawnRotation = AvatarActor->GetActorRotation();

        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = AvatarActor;
        SpawnParams.Instigator = Cast<APawn>(AvatarActor);
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossible;

        ALB_EnemyCharacter* SpawnedMinion = World->SpawnActor<ALB_EnemyCharacter>(
            ClassToSpawn, SpawnLocation, SpawnRotation, SpawnParams);

        if (!SpawnedMinion)
        {
            UE_LOG(LogTemp, Warning, TEXT("[%s] 소환 실패: %s"), *GetName(), *ClassToSpawn->GetName());
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

        ActiveMinions.Add(SpawnedMinion);
        ++SpawnedCount;
    }

    UE_LOG(LogTemp, Log, TEXT("[%s] 총 %d마리 소환 완료 (현재 활성: %d/%d)"),
        *GetName(), SpawnedCount, ActiveMinions.Num(), SummonSpawnParams.MaxActiveMinions);*/
	
	
	EndAbility(GetCurrentAbilitySpecHandle()
	,GetCurrentActorInfo()
	,GetCurrentActivationInfo()
	,true
	,false);
}

void ULB_SummonAbility::OnAbilityCancelled()
{
	EndAbility(GetCurrentAbilitySpecHandle()
	,GetCurrentActorInfo()
	,GetCurrentActivationInfo()
	,true
	,true);
}


