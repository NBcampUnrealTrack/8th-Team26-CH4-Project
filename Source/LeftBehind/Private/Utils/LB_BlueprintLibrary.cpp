// Fill out your copyright notice in the Description page of Project Settings.


#include "Utils/LB_BlueprintLibrary.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/LB_AttributeSet.h"
#include "Characters/LB_BaseCharacter.h"
#include "Characters/LB_EnemyCharacter.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "GameplayTags/LBTags.h"
#include "Kismet/GameplayStatics.h"

EHitDirection ULB_BlueprintLibrary::GetHitDirection(const FVector& TargetForward, const FVector& ToInstigator)
{
	const float Dot = FVector::DotProduct(TargetForward, ToInstigator);
	if (Dot < -0.5f)
	{
		return EHitDirection::Back;
	}
	if (Dot < 0.5f)
	{
		// Either Left or Right
		const FVector Cross = FVector::CrossProduct(TargetForward, ToInstigator);
		if (Cross.Z < 0.f)
		{
			return EHitDirection::Left;
		}
		return EHitDirection::Right;
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
		ALB_BaseCharacter* BaseCharacter = Cast<ALB_BaseCharacter>(Actor);
		if (!IsValid(BaseCharacter) || !BaseCharacter->IsAlive()) continue;

		const float Distance = FVector::Dist(Origin, Actor->GetActorLocation());
		if (ALB_BaseCharacter* SearchingCharacter = Cast<ALB_BaseCharacter>(WorldContextObject); IsValid(SearchingCharacter))
		{
			if (Distance > SearchingCharacter->SearchRange) continue;
		}
		if (Distance < ClosestDistance)
		{
			ClosestDistance = Distance;
			ClosestActor = Actor;
		}
	}

	FClosestActorWithTagResult Result;
	Result.Actor = ClosestActor;
	Result.Distance = ClosestDistance;

	return Result;
}

void ULB_BlueprintLibrary::SendDamageEventToPlayer(AActor* Target, const TSubclassOf<UGameplayEffect>& DamageEffect,
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
	ApplyDamageEffect_ServerOnly(SourceActor, Target, DamageEffect, Payload, DataTag, Damage, ResolvedEventTag, OptionalParticleSystem);
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

	// 체력의 원본은 서버다. 클라이언트에서 호출되면 이 함수는 아무 값도 바꾸지 않는다.
	if (!Target->HasAuthority())
	{
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

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	UAbilitySystemComponent* SourceASC = IsValid(Source) ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Source) : nullptr;
	if (!IsValid(TargetASC))
	{
		return false;
	}

	if (!IsValid(SourceASC))
	{
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
		return false;
	}

	const FGameplayTag MagnitudeTag =
		(DataTag.IsValid() && !DataTag.MatchesTagExact(LBTags::None))
		? DataTag
		: LBTags::SetByCaller::Damage;

	// GameplayEffect 안의 SetByCaller 값은 보통 "더하기"로 계산된다. 그래서 피해는 음수로 넣어 Health를 줄인다.
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
	return true;
}

TArray<AActor*> ULB_BlueprintLibrary::HitBoxOverlapTest(AActor* AvatarActor, float HitBoxRadius,
	float HitBoxForwardOffset, float HitBoxElevationOffset, bool bDrawDebugs)
{
	if (!IsValid(AvatarActor)) return TArray<AActor*>();

	// Ensure that the overlap test ignores the Avatar Actor
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(AvatarActor);

	FCollisionResponseParams ResponseParams;
	ResponseParams.CollisionResponse.SetAllChannels(ECR_Ignore);
	ResponseParams.CollisionResponse.SetResponse(ECC_Pawn, ECR_Block);

	TArray<FOverlapResult> OverlapResults;
	FCollisionShape Sphere = FCollisionShape::MakeSphere(HitBoxRadius);

	const FVector Forward = AvatarActor->GetActorForwardVector() * HitBoxForwardOffset;
	const FVector HitBoxLocation = AvatarActor->GetActorLocation() + Forward + FVector(0.f, 0.f, HitBoxElevationOffset);

	UWorld* World = GEngine->GetWorldFromContextObject(AvatarActor, EGetWorldErrorMode::LogAndReturnNull);
	if (!IsValid(World)) return TArray<AActor*>();
	World->OverlapMultiByChannel(OverlapResults, HitBoxLocation, FQuat::Identity, ECC_Visibility, Sphere, QueryParams, ResponseParams);

	TArray<AActor*> ActorsHit;
	for (const FOverlapResult& Result : OverlapResults)
	{
		ALB_BaseCharacter* BaseCharacter = Cast<ALB_BaseCharacter>(Result.GetActor());
		if (!IsValid(BaseCharacter)) continue;
		if (!BaseCharacter->IsAlive()) continue;
		ActorsHit.AddUnique(BaseCharacter);
	}

	if (bDrawDebugs)
	{
		DrawHitBoxOverlapDebugs(AvatarActor, OverlapResults, HitBoxLocation, HitBoxRadius);
	}

	return ActorsHit;
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
			const FVector2D FalloffRange(InnerRadius, OuterRadius); // input range
			const FVector2D LaunchForceRange(LaunchForceMagnitude, 0.f); // output range
			LaunchForce = FMath::GetMappedRangeValueClamped(FalloffRange, LaunchForceRange, Distance);
		}
		if (bDrawDebugs) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, FString::Printf(TEXT("LaunchForce: %f"), LaunchForce));

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
