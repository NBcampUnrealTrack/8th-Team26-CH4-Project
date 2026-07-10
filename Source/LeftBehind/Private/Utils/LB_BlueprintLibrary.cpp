// Fill out your copyright notice in the Description page of Project Settings.

#include "Utils/LB_BlueprintLibrary.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystem/LB_AttributeSet.h"
#include "Characters/LB_BaseCharacter.h"
#include "Characters/LB_EnemyCharacter.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameState/LB_RaidGameState.h"
#include "GameplayTags/LBTags.h"
#include "Kismet/GameplayStatics.h"
#include "Characters/Boss/LB_BossCharacter.h"

EHitDirection ULB_BlueprintLibrary::GetHitDirection(const FVector& TargetForward, const FVector& ToInstigator)
{
	const float Dot = FVector::DotProduct(TargetForward, ToInstigator);
	if (Dot < -0.5f)
	{
		return EHitDirection::Back;
	}
	if (Dot < 0.5f)
	{
		const FVector Cross = FVector::CrossProduct(TargetForward, ToInstigator);
		return Cross.Z < 0.f ? EHitDirection::Left : EHitDirection::Right;
	}
	return EHitDirection::Forward;
}

FName ULB_BlueprintLibrary::GetHitDirectionName(const EHitDirection& HitDirection)
{
	switch (HitDirection)
	{
	case EHitDirection::Left: return FName("Left");
	case EHitDirection::Right: return FName("Right");
	case EHitDirection::Forward: return FName("Forward");
	case EHitDirection::Back: return FName("Back");
	default: return FName("None");
	}
}

FClosestActorWithTagResult ULB_BlueprintLibrary::FindClosestActorWithTag(UObject* WorldContextObject,
	const FVector& Origin, const FName& Tag, float SearchRange)
{
	TArray<AActor*> ActorsWithTag;
	UGameplayStatics::GetAllActorsWithTag(WorldContextObject, Tag, ActorsWithTag);

	float ClosestDistance = TNumericLimits<float>::Max();
	AActor* ClosestActor = nullptr;

	for (AActor* Actor : ActorsWithTag)
	{
		if (!IsValid(Actor)) continue;

		const ALB_BaseCharacter* BaseCharacter = Cast<ALB_BaseCharacter>(Actor);
		if (!IsValid(BaseCharacter) || !BaseCharacter->IsAlive()) continue;

		const float Distance = FVector::Dist(Origin, Actor->GetActorLocation());
		if (const ALB_BaseCharacter* SearchingCharacter = Cast<ALB_BaseCharacter>(WorldContextObject); IsValid(SearchingCharacter))
		{
			if (Distance > SearchingCharacter->SearchRange) continue;
		}
		else if (SearchRange > 0.f && Distance > SearchRange)
		{
			continue;
		}

		if (Distance < ClosestDistance)
		{
			ClosestDistance = Distance;
			ClosestActor = Actor;
		}
	}

	FClosestActorWithTagResult Result;
	Result.Actor = ClosestActor;
	Result.Distance = ClosestActor ? ClosestDistance : 0.f;
	return Result;
}

bool ULB_BlueprintLibrary::SendDamageEventToPlayer(AActor* Target, const TSubclassOf<UGameplayEffect>& DamageEffect,
	FGameplayEventData& Payload, const FGameplayTag& DataTag, float Damage, const FGameplayTag& EventTagOverride,
	UObject* OptionalParticleSystem)
{
	FGameplayTag ResolvedEventTag = EventTagOverride;

	if (!ResolvedEventTag.IsValid() || ResolvedEventTag.MatchesTagExact(LBTags::None))
	{
		if (const ALB_BaseCharacter* PlayerCharacter = Cast<ALB_BaseCharacter>(Target))
		{
			const ULB_AttributeSet* AttributeSet = Cast<ULB_AttributeSet>(PlayerCharacter->GetAttributeSet());
			if (IsValid(AttributeSet))
			{
				const bool bLethal = AttributeSet->GetHealth() - FMath::Abs(Damage) <= 0.f;
				ResolvedEventTag = bLethal ? LBTags::Events::Player::Death : LBTags::Events::Player::HitReact;
			}
		}
	}

	AActor* SourceActor = const_cast<AActor*>(Payload.Instigator.Get());
	return ApplyDamageEffect_ServerOnly(SourceActor, Target, DamageEffect, Payload, DataTag, Damage, ResolvedEventTag, OptionalParticleSystem);
}

void ULB_BlueprintLibrary::SendDamageEventToPlayers(TArray<AActor*> Targets,
	const TSubclassOf<UGameplayEffect>& DamageEffect, FGameplayEventData& Payload, const FGameplayTag& DataTag,
	float Damage, const FGameplayTag& EventTagOverride, UObject* OptionalParticleSystem)
{
	for (AActor* Target : Targets)
	{
		SendDamageEventToPlayer(Target, DamageEffect, Payload, DataTag, Damage, EventTagOverride, OptionalParticleSystem);
	}
}

bool ULB_BlueprintLibrary::ApplyDamageEffect_ServerOnly(AActor* Source, AActor* Target,
	const TSubclassOf<UGameplayEffect>& DamageEffect, FGameplayEventData& Payload, const FGameplayTag& DataTag,
	float Damage, const FGameplayTag& EventTagOverride, UObject* OptionalParticleSystem)
{
	if (!IsValid(Target) || !DamageEffect)
	{
		return false;
	}

	// 체력의 원본은 서버다. 클라이언트에서 이 함수를 불러도 HP는 절대 바꾸지 않는다.
	if (!Target->HasAuthority())
	{
		return false;
	}

	if (!CanActorDamageTarget(Source, Target))
	{
		UE_LOG(LogTemp, Verbose, TEXT("[LB Combat] Friendly fire blocked. Source=%s Target=%s"), *GetNameSafe(Source), *GetNameSafe(Target));
		return false;
	}

	const float SafeDamage = FMath::Abs(Damage);
	if (SafeDamage <= 0.f)
	{
		return false;
	}

	if (const ALB_BaseCharacter* TargetCharacter = Cast<ALB_BaseCharacter>(Target); IsValid(TargetCharacter) && !TargetCharacter->IsAlive())
	{
		return false;
	}

	if (const ALB_BossCharacter* RaidBoss = Cast<ALB_BossCharacter>(Target); IsValid(RaidBoss) && RaidBoss->IsDead())
	{
		return false;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	if (!IsValid(TargetASC))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LB Combat] Damage failed. Target has no ASC. Target=%s"), *GetNameSafe(Target));
		return false;
	}

	UAbilitySystemComponent* SourceASC = IsValid(Source) ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Source) : nullptr;
	if (!IsValid(SourceASC))
	{
		// 설치물/환경 피해처럼 공격자 ASC가 없을 때도 Target ASC를 빌려 GE 스펙을 만들 수 있다.
		SourceASC = TargetASC;
	}

	FGameplayEffectContextHandle ContextHandle = SourceASC->MakeEffectContext();
	if (IsValid(Source))
	{
		ContextHandle.AddSourceObject(Source);
		ContextHandle.AddInstigator(Source, Source);
	}

	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(DamageEffect, 1.f, ContextHandle);
	if (!SpecHandle.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LB Combat] Damage failed. Invalid GameplayEffect spec. Effect=%s"), *GetNameSafe(DamageEffect.Get()));
		return false;
	}

	const FGameplayTag MagnitudeTag =
		(DataTag.IsValid() && !DataTag.MatchesTagExact(LBTags::None))
		? DataTag
		: LBTags::SetByCaller::Damage;

	// GE_Damage는 Health에 Additive로 들어간다. 피해량을 음수로 넣어 Health를 줄인다.
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, MagnitudeTag, -SafeDamage);

	if (EventTagOverride.IsValid() && !EventTagOverride.MatchesTagExact(LBTags::None))
	{
		Payload.OptionalObject = OptionalParticleSystem;
		Payload.Target = Target;
		if (IsValid(Source))
		{
			Payload.Instigator = Source;
		}

		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Target, EventTagOverride, Payload);
	}

	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	UE_LOG(LogTemp, Log, TEXT("[LB Combat] %s damaged %s for %.1f"), *GetNameSafe(Source), *GetNameSafe(Target), SafeDamage);
	return true;
}

bool ULB_BlueprintLibrary::IsPlayerControlledCombatActor(const AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return false;
	}

	const APawn* Pawn = Cast<APawn>(Actor);
	if (!IsValid(Pawn))
	{
		return Actor->IsA<APlayerState>();
	}

	// 서버 공격 판정에서는 PlayerState 또는 PlayerController 중 하나만 있어도 플레이어 Pawn으로 본다.
	if (IsValid(Pawn->GetPlayerState()))
	{
		return true;
	}

	const AController* Controller = Pawn->GetController();
	return IsValid(Controller) && Controller->IsPlayerController();
}

bool ULB_BlueprintLibrary::CanActorDamageTarget(const AActor* Source, const AActor* Target)
{
	if (!IsValid(Target))
	{
		return false;
	}

	if (Source == Target)
	{
		return false;
	}

	// PvE 레이드 규칙: 플레이어끼리만 데미지를 막는다.
	// 보스/적이 플레이어를 공격하는 흐름은 유지해야 하므로 양쪽이 플레이어일 때만 차단한다.
	return !(IsPlayerControlledCombatActor(Source) && IsPlayerControlledCombatActor(Target));
}

TArray<AActor*> ULB_BlueprintLibrary::FindDamageableActorsInHitBox(AActor* AvatarActor, float HitBoxRadius,
	float HitBoxForwardOffset, float HitBoxElevationOffset, bool bDrawDebugs)
{
	TArray<AActor*> ActorsHit;
	if (!IsValid(AvatarActor)) return ActorsHit;

	UWorld* World = AvatarActor->GetWorld();
	if (!IsValid(World)) return ActorsHit;

	const float SafeRadius = FMath::Max(1.f, HitBoxRadius);
	const FVector HitBoxLocation =
		AvatarActor->GetActorLocation()
		+ AvatarActor->GetActorForwardVector() * HitBoxForwardOffset
		+ FVector(0.f, 0.f, HitBoxElevationOffset);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(LBFindDamageableActorsInHitBox), false);
	QueryParams.AddIgnoredActor(AvatarActor);

	FCollisionObjectQueryParams ObjectQueryParams;
	// 에디터에서 보스 캡슐 Object Type이 Pawn이 아니어도 공격 판정이 닿게 넓게 찾고,
	// 실제 데미지는 아래의 ASC 보유 여부로 한 번 더 걸러 안전하게 적용한다.
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);

	TArray<FOverlapResult> OverlapResults;
	const FCollisionShape Sphere = FCollisionShape::MakeSphere(SafeRadius);
	World->OverlapMultiByObjectType(OverlapResults, HitBoxLocation, FQuat::Identity, ObjectQueryParams, Sphere, QueryParams);

	for (const FOverlapResult& Result : OverlapResults)
	{
		AActor* HitActor = Result.GetActor();
		if (!IsValid(HitActor) || HitActor == AvatarActor)
		{
			continue;
		}

		// 전투 대상은 ASC가 있어야 한다. 이 조건 하나로 플레이어, 일반 적, 레이드 보스를 같은 방식으로 다룬다.
		if (!CanActorDamageTarget(AvatarActor, HitActor))
		{
			continue;
		}

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
		if (!IsValid(TargetASC))
		{
			continue;
		}

		if (const ALB_BaseCharacter* BaseCharacter = Cast<ALB_BaseCharacter>(HitActor); IsValid(BaseCharacter) && !BaseCharacter->IsAlive())
		{
			continue;
		}

		if (const ALB_BossCharacter* RaidBoss = Cast<ALB_BossCharacter>(HitActor); IsValid(RaidBoss) && RaidBoss->IsDead())
		{
			continue;
		}

		ActorsHit.AddUnique(HitActor);
	}

	if (bDrawDebugs && ActorsHit.IsEmpty())
	{
		const FString Message = FString::Printf(TEXT("[LB Combat] Hit box found no ASC target. Avatar=%s Radius=%.1f Forward=%.1f Elevation=%.1f"),
			*GetNameSafe(AvatarActor),
			SafeRadius,
			HitBoxForwardOffset,
			HitBoxElevationOffset);
		UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);

		if (AvatarActor->HasAuthority())
		{
			if (ALB_RaidGameState* RaidGameState = World->GetGameState<ALB_RaidGameState>())
			{
				RaidGameState->MulticastRaidDebugMessage(Message, FColor::Orange, 2.f);
			}
		}
	}

	if (bDrawDebugs)
	{
		if (AvatarActor->HasAuthority())
		{
			if (ALB_RaidGameState* RaidGameState = World->GetGameState<ALB_RaidGameState>())
			{
				RaidGameState->MulticastRaidDebugSphere(HitBoxLocation, SafeRadius, FColor::Red, 2.f);
				for (AActor* HitActor : ActorsHit)
				{
					RaidGameState->MulticastRaidDebugSphere(HitActor->GetActorLocation() + FVector(0.f, 0.f, 100.f), 30.f, FColor::Green, 2.f);
				}
				return ActorsHit;
			}
		}

		DrawDebugSphere(World, HitBoxLocation, SafeRadius, 16, FColor::Red, false, 2.f);
		for (AActor* HitActor : ActorsHit)
		{
			DrawDebugSphere(World, HitActor->GetActorLocation() + FVector(0.f, 0.f, 100.f), 30.f, 10, FColor::Green, false, 2.f);
		}
	}

	return ActorsHit;
}

TArray<AActor*> ULB_BlueprintLibrary::HitBoxOverlapTest(AActor* AvatarActor, float HitBoxRadius,
	float HitBoxForwardOffset, float HitBoxElevationOffset, bool bDrawDebugs)
{
	return FindDamageableActorsInHitBox(AvatarActor, HitBoxRadius, HitBoxForwardOffset, HitBoxElevationOffset, bDrawDebugs);
}

void ULB_BlueprintLibrary::DrawHitBoxOverlapDebugs(const UObject* WorldContextObject,
	const TArray<FOverlapResult>& OverlapResults, const FVector& HitBoxLocation, float HitBoxRadius)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!IsValid(World)) return;

	DrawDebugSphere(World, HitBoxLocation, HitBoxRadius, 16, FColor::Red, false, 3.f);

	for (const FOverlapResult& Result : OverlapResults)
	{
		if (IsValid(Result.GetActor()))
		{
			FVector DebugLocation = Result.GetActor()->GetActorLocation();
			DebugLocation.Z += 100.f;
			DrawDebugSphere(World, DebugLocation, 30.f, 10, FColor::Green, false, 3.f);
		}
	}
}

TArray<AActor*> ULB_BlueprintLibrary::ApplyKnockback(AActor* AvatarActor, const TArray<AActor*>& HitActors,
	float InnerRadius, float OuterRadius, float LaunchForceMagnitude, float RotationAngle, bool bDrawDebugs)
{
	for (AActor* HitActor : HitActors)
	{
		ACharacter* HitCharacter = Cast<ACharacter>(HitActor);
		if (!IsValid(HitCharacter) || !IsValid(AvatarActor)) return TArray<AActor*>();

		const FVector HitCharacterLocation = HitCharacter->GetActorLocation();
		const FVector AvatarLocation = AvatarActor->GetActorLocation();
		const FVector ToHitActor = HitCharacterLocation - AvatarLocation;
		const float Distance = FVector::Dist(AvatarLocation, HitCharacterLocation);

		float LaunchForce = 0.f;
		if (Distance > OuterRadius) continue;
		if (Distance <= InnerRadius)
		{
			LaunchForce = LaunchForceMagnitude;
		}
		else
		{
			const FVector2D FalloffRange(InnerRadius, OuterRadius);
			const FVector2D LaunchForceRange(LaunchForceMagnitude, 0.f);
			LaunchForce = FMath::GetMappedRangeValueClamped(FalloffRange, LaunchForceRange, Distance);
		}
		if (bDrawDebugs)
		{
			UE_LOG(LogTemp, Log, TEXT("[LB Combat] LaunchForce=%.2f Target=%s"), LaunchForce, *GetNameSafe(HitActor));
		}

		FVector KnockbackForce = ToHitActor.GetSafeNormal();
		KnockbackForce.Z = 0.f;

		const FVector Right = KnockbackForce.RotateAngleAxis(90.f, FVector::UpVector);
		KnockbackForce = KnockbackForce.RotateAngleAxis(-RotationAngle, Right) * LaunchForce;

		if (bDrawDebugs)
		{
			UWorld* World = GEngine->GetWorldFromContextObject(AvatarActor, EGetWorldErrorMode::LogAndReturnNull);
			DrawDebugDirectionalArrow(World, HitCharacterLocation, HitCharacterLocation + KnockbackForce, 100.f, FColor::Green, false, 3.f);
		}

		if (ALB_EnemyCharacter* EnemyCharacter = Cast<ALB_EnemyCharacter>(HitCharacter); IsValid(EnemyCharacter))
		{
			EnemyCharacter->StopMovementUntilLanded();
		}

		HitCharacter->LaunchCharacter(KnockbackForce, true, true);
	}
	return HitActors;
}
