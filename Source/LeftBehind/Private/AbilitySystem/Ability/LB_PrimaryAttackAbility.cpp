// LB_PrimaryAttackAbility.cpp

#include "AbilitySystem/Ability/LB_PrimaryAttackAbility.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Characters/LB_BaseCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameplayTags/LBTags.h"
#include "UObject/ConstructorHelpers.h"
#include "Utils/LB_BlueprintLibrary.h"

ULB_PrimaryAttackAbility::ULB_PrimaryAttackAbility()
{
	// 데미지 계산은 서버만 수행한다. 클라이언트가 직접 HP를 바꾸면 멀티플레이에서 치트와 불일치가 생긴다.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// BP에서 태그를 빼먹어도 Primary 입력으로 이 Ability를 찾을 수 있게 기본 태그를 넣는다.
	FGameplayTagContainer DefaultAbilityTags;
	DefaultAbilityTags.AddTag(LBTags::LBAbilities::Primary);
	SetAssetTags(DefaultAbilityTags);

	// 기본 프로젝트 에셋을 코드 기본값으로 잡아두고, 에디터 Class Defaults에서 언제든 교체할 수 있게 한다.
	static ConstructorHelpers::FObjectFinder<UAnimMontage> MontageAAsset(
		TEXT("/Game/LeftBehind/Characters/Animations/LB_AM_Primary_A.LB_AM_Primary_A"));
	if (MontageAAsset.Succeeded())
	{
		PrimaryMontageA = MontageAAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> MontageBAsset(
		TEXT("/Game/LeftBehind/Characters/Animations/LB_AM_Primary_B.LB_AM_Primary_B"));
	if (MontageBAsset.Succeeded())
	{
		PrimaryMontageB = MontageBAsset.Object;
	}
}

void ULB_PrimaryAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
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

	if (!DamageEffect)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LB PrimaryAttack] DamageEffect is not set. GA=%s"), *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	PlayPrimaryMontage(AvatarActor);

	TArray<AActor*> HitActors = ULB_BlueprintLibrary::FindDamageableActorsInHitBox(
		AvatarActor,
		HitBoxRadius,
		HitBoxForwardOffset,
		HitBoxElevationOffset,
		bDrawHitDebug
	);

	if (HitActors.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LB PrimaryAttack] No damageable target. Avatar=%s"), *GetNameSafe(AvatarActor));
	}

	FGameplayEventData DamagePayload;
	DamagePayload.Instigator = AvatarActor;
	DamagePayload.EventTag = LBTags::LBAbilities::Primary;
	DamagePayload.EventMagnitude = Damage;

	int32 AppliedCount = 0;
	for (AActor* HitActor : HitActors)
	{
		// ApplyDamageEffect_ServerOnly를 직접 부르는 대신 SendDamageEventToPlayer를 써서,
		// 치명타 여부에 따라 LBTags.Events.Player.Death / HitReact를 대상에게 자동으로 보낸다.
		if (ULB_BlueprintLibrary::SendDamageEventToPlayer(
			HitActor,
			DamageEffect,
			DamagePayload,
			LBTags::SetByCaller::Damage,
			Damage,
			LBTags::None))
		{
			++AppliedCount;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[LB PrimaryAttack] %s hit %d actor(s). Damage=%.1f"), *GetNameSafe(AvatarActor), AppliedCount, Damage);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

UAnimMontage* ULB_PrimaryAttackAbility::SelectNextPrimaryMontage()
{
	const bool bUseFirstMontage = NextMontageIndex % 2 == 0;
	UAnimMontage* SelectedMontage = bUseFirstMontage ? PrimaryMontageA.Get() : PrimaryMontageB.Get();

	// 한쪽 몽타주가 비어 있어도 공격이 멈추지 않도록 다른 쪽으로 대체한다.
	if (!IsValid(SelectedMontage))
	{
		SelectedMontage = bUseFirstMontage ? PrimaryMontageB.Get() : PrimaryMontageA.Get();
	}

	NextMontageIndex = (NextMontageIndex + 1) % 2;
	return SelectedMontage;
}

void ULB_PrimaryAttackAbility::PlayPrimaryMontage(AActor* AvatarActor)
{
	UAnimMontage* SelectedMontage = SelectNextPrimaryMontage();
	if (!IsValid(SelectedMontage))
	{
		UE_LOG(LogTemp, Warning, TEXT("[LB PrimaryAttack] No primary montage is set. Avatar=%s"), *GetNameSafe(AvatarActor));
		return;
	}

	if (ALB_BaseCharacter* BaseCharacter = Cast<ALB_BaseCharacter>(AvatarActor))
	{
		BaseCharacter->MulticastPlayCosmeticMontage(SelectedMontage, MontagePlayRate);
		UE_LOG(LogTemp, Log, TEXT("[LB PrimaryAttack] Montage=%s Avatar=%s"), *GetNameSafe(SelectedMontage), *GetNameSafe(AvatarActor));
		return;
	}

	// 혹시 BaseCharacter가 아닌 Character가 이 Ability를 써도 서버 화면에서는 최소한 재생되게 하는 예비 경로다.
	ACharacter* Character = Cast<ACharacter>(AvatarActor);
	USkeletalMeshComponent* MeshComponent = IsValid(Character) ? Character->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = IsValid(MeshComponent) ? MeshComponent->GetAnimInstance() : nullptr;
	if (IsValid(AnimInstance))
	{
		AnimInstance->Montage_Play(SelectedMontage, FMath::Max(0.01f, MontagePlayRate));
	}
}
