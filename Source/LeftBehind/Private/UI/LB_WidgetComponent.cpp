// LB_WidgetComponent.cpp

#include "UI/LB_WidgetComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystem/LB_AttributeSet.h"
#include "Blueprint/WidgetTree.h"
#include "Characters/LB_BaseCharacter.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "UI/LB_AttributeWidget.h"

void ULB_WidgetComponent::OnRegister()
{
	Super::OnRegister();

	ApplyOwnerCenteredPlacement();
}

void ULB_WidgetComponent::BeginPlay()
{
	Super::BeginPlay();

	ApplyOwnerCenteredPlacement();
	InitAbilitySystemData();

	if (!IsASCInitialized())
	{
		if (BaseCharacterOwner.IsValid())
		{
			// 플레이어는 PlayerState의 ASC가 Possess 이후 준비될 수 있으므로 완료 신호를 기다린다.
			BaseCharacterOwner->OnAscInitialized.AddUniqueDynamic(this, &ThisClass::OnASCInitialized);
		}
		return;
	}

	InitializeAttributeDelegate();
}

void ULB_WidgetComponent::ApplyOwnerCenteredPlacement()
{
	if (!bAutoPlaceAboveOwnerCenter)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return;
	}

	FVector LocalCenter = FVector::ZeroVector;
	float HalfHeight = 0.f;

	if (const ACharacter* CharacterOwner = Cast<ACharacter>(OwnerActor))
	{
		if (const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent())
		{
			// Character의 기준점은 보통 캡슐 중앙이다. X/Y를 0으로 맞춰 보스 중앙선 위에 HP를 둔다.
			HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		}
	}

	if (HalfHeight <= 0.f)
	{
		FVector BoundsOrigin = FVector::ZeroVector;
		FVector BoundsExtent = FVector::ZeroVector;
		OwnerActor->GetActorBounds(false, BoundsOrigin, BoundsExtent);

		// Character가 아닌 액터도 지원하기 위해 월드 바운딩 박스 중심을 로컬 좌표로 변환한다.
		LocalCenter = OwnerActor->GetActorTransform().InverseTransformPosition(BoundsOrigin);
		HalfHeight = BoundsExtent.Z;
	}

	SetPivot(FVector2D(0.5f, 0.5f));
	SetRelativeLocation(FVector(LocalCenter.X, LocalCenter.Y, LocalCenter.Z + HalfHeight + AboveOwnerCenterOffset));
}

void ULB_WidgetComponent::InitAbilitySystemData()
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return;
	}

	BaseCharacterOwner = Cast<ALB_BaseCharacter>(OwnerActor);
	if (BaseCharacterOwner.IsValid())
	{
		AttributeSet = Cast<ULB_AttributeSet>(BaseCharacterOwner->GetAttributeSet());
		AbilitySystemComponent = BaseCharacterOwner->GetAbilitySystemComponent();
		return;
	}

	// RaidBossBase처럼 BaseCharacter가 아니어도 ASC를 가진 액터라면 같은 HP 위젯을 사용할 수 있다.
	const IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(OwnerActor);
	if (!AbilityOwner)
	{
		return;
	}

	AbilitySystemComponent = AbilityOwner->GetAbilitySystemComponent();
	if (AbilitySystemComponent.IsValid())
	{
		AttributeSet = const_cast<ULB_AttributeSet*>(AbilitySystemComponent->GetSet<ULB_AttributeSet>());
	}
}

bool ULB_WidgetComponent::IsASCInitialized() const
{
	return AbilitySystemComponent.IsValid() && AttributeSet.IsValid();
}

void ULB_WidgetComponent::InitializeAttributeDelegate()
{
	if (!IsASCInitialized())
	{
		return;
	}

	// 위젯이 먼저 만들어지고 Attribute가 나중에 채워질 수 있으므로 먼저 연결한 뒤 현재값을 다시 넣는다.
	BindToAttributeChanges();

	if (!AttributeSet->bAttributeInitialized)
	{
		AttributeSet->OnAttributesInitialized.AddUniqueDynamic(this, &ThisClass::BindToAttributeChanges);
	}
}

void ULB_WidgetComponent::OnASCInitialized(UAbilitySystemComponent* ASC, UAttributeSet* AS)
{
	AbilitySystemComponent = ASC;
	AttributeSet = Cast<ULB_AttributeSet>(AS);

	if (!IsASCInitialized())
	{
		return;
	}

	InitializeAttributeDelegate();
}

void ULB_WidgetComponent::BindToAttributeChanges()
{
	if (!IsASCInitialized())
	{
		return;
	}

	UUserWidget* RootWidget = GetUserWidgetObject();
	if (!IsValid(RootWidget) || !RootWidget->WidgetTree)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LB Widget] WidgetComponent has no valid widget. Owner=%s"), *GetNameSafe(GetOwner()));
		return;
	}

	if (AttributeMap.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LB Widget] AttributeMap is empty. Owner=%s WidgetClass=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(RootWidget->GetClass()));
		return;
	}

	if (bHasBoundToAttributeChanges)
	{
		// 이미 델리게이트는 연결되어 있다면 중복 연결하지 않고 현재 HP/Mana만 화면에 다시 반영한다.
		PushCurrentAttributeValuesToWidgets(RootWidget);
		return;
	}

	int32 BoundWidgetCount = 0;
	for (const TTuple<FGameplayAttribute, FGameplayAttribute>& Pair : AttributeMap)
	{
		if (BindWidgetToAttributeChanges(RootWidget, Pair))
		{
			++BoundWidgetCount;
		}

		RootWidget->WidgetTree->ForEachWidget([this, Pair, &BoundWidgetCount](UWidget* ChildWidget)
		{
			if (BindWidgetToAttributeChanges(ChildWidget, Pair))
			{
				++BoundWidgetCount;
			}
		});
	}

	if (BoundWidgetCount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LB Widget] No LB_AttributeWidget matched. Owner=%s WidgetClass=%s. Check Widget Class, Parent Class, Attribute, MaxAttribute."),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(RootWidget->GetClass()));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[LB Widget] Bound %d attribute widget(s). Owner=%s WidgetClass=%s"),
		BoundWidgetCount,
		*GetNameSafe(GetOwner()),
		*GetNameSafe(RootWidget->GetClass()));

	bHasBoundToAttributeChanges = true;
}

void ULB_WidgetComponent::PushCurrentAttributeValuesToWidgets(UUserWidget* RootWidget) const
{
	if (!IsValid(RootWidget) || !RootWidget->WidgetTree || !AttributeSet.IsValid())
	{
		return;
	}

	for (const TTuple<FGameplayAttribute, FGameplayAttribute>& Pair : AttributeMap)
	{
		PushWidgetCurrentAttributeValue(RootWidget, Pair);

		RootWidget->WidgetTree->ForEachWidget([this, Pair](UWidget* ChildWidget)
		{
			PushWidgetCurrentAttributeValue(ChildWidget, Pair);
		});
	}
}

bool ULB_WidgetComponent::PushWidgetCurrentAttributeValue(UWidget* WidgetObject, const TTuple<FGameplayAttribute, FGameplayAttribute>& Pair) const
{
	ULB_AttributeWidget* AttributeWidget = Cast<ULB_AttributeWidget>(WidgetObject);
	if (!IsValid(AttributeWidget) || !AttributeWidget->MatchesAttributes(Pair))
	{
		return false;
	}

	// 델리게이트 이벤트가 없어도 현재 Attribute 값을 직접 넣어 시작 HP/Mana 표시를 보장한다.
	AttributeWidget->OnAttributeChange(Pair, AttributeSet.Get());
	return true;
}

bool ULB_WidgetComponent::BindWidgetToAttributeChanges(UWidget* WidgetObject, const TTuple<FGameplayAttribute, FGameplayAttribute>& Pair) const
{
	ULB_AttributeWidget* AttributeWidget = Cast<ULB_AttributeWidget>(WidgetObject);
	if (!IsValid(AttributeWidget) || !AttributeWidget->MatchesAttributes(Pair))
	{
		return false;
	}

	// 바인딩 직후 현재값을 한 번 넣어 전투 시작 시 HP/Mana가 바로 보이게 한다.
	AttributeWidget->OnAttributeChange(Pair, AttributeSet.Get());

	TWeakObjectPtr<ULB_AttributeWidget> WeakAttributeWidget = AttributeWidget;
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Pair.Key).AddLambda(
		[this, WeakAttributeWidget, Pair](const FOnAttributeChangeData& AttributeChangeData)
		{
			if (!WeakAttributeWidget.IsValid() || !AttributeSet.IsValid())
			{
				return;
			}

			WeakAttributeWidget->OnAttributeChange(Pair, AttributeSet.Get());
		}
	);

	return true;
}
