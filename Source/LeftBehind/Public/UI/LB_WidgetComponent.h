// LB_WidgetComponent.h

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "Components/WidgetComponent.h"
#include "LB_WidgetComponent.generated.h"


class UAbilitySystemComponent;
class ULB_AttributeSet;
class ALB_BaseCharacter;
class UUserWidget;
class UWidget;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LEFTBEHIND_API ULB_WidgetComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:

protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	
	UPROPERTY(EditAnywhere)
	TMap<FGameplayAttribute, FGameplayAttribute> AttributeMap;

	// true면 위젯을 소유 액터의 중앙선 위에 자동 배치한다. 보스 HP 바가 몸 중심에서 벗어나지 않게 하기 위한 옵션이다.
	UPROPERTY(EditAnywhere, Category = "LB|Widget Placement")
	bool bAutoPlaceAboveOwnerCenter = true;

	// 캡슐/바운딩 박스 꼭대기에서 추가로 올릴 높이다. 보스 모델 크기에 맞춰 BP에서 조정할 수 있다.
	UPROPERTY(EditAnywhere, Category = "LB|Widget Placement", meta = (ClampMin = "0.0"))
	float AboveOwnerCenterOffset = 40.f;

private:
	TWeakObjectPtr<ALB_BaseCharacter> BaseCharacterOwner;
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	TWeakObjectPtr<ULB_AttributeSet> AttributeSet;
	bool bHasBoundToAttributeChanges = false;

	void InitAbilitySystemData();
	bool IsASCInitialized() const;
	void ApplyOwnerCenteredPlacement();
	void InitializeAttributeDelegate();
	void PushCurrentAttributeValuesToWidgets(UUserWidget* RootWidget) const;
	bool PushWidgetCurrentAttributeValue(UWidget* WidgetObject, const TTuple<FGameplayAttribute, FGameplayAttribute>& Pair) const;
	bool BindWidgetToAttributeChanges(UWidget* WidgetObject, const TTuple<FGameplayAttribute, FGameplayAttribute>& Pair) const;

	UFUNCTION()
	void OnASCInitialized(UAbilitySystemComponent* ASC, UAttributeSet* AS);
	
	UFUNCTION()
	void BindToAttributeChanges();
};
