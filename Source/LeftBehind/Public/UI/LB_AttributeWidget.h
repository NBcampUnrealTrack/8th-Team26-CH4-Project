// LB_AttributeWidget.h

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AbilitySystem/LB_AttributeSet.h"

#include "LB_AttributeWidget.generated.h"

UCLASS()
class LEFTBEHIND_API ULB_AttributeWidget : public UUserWidget
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crash|Attributes")
	FGameplayAttribute Attribute;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crash|Attributes")
	FGameplayAttribute MaxAttribute;

	void OnAttributeChange(const TTuple<FGameplayAttribute, FGameplayAttribute>& Pair, ULB_AttributeSet* AttributeSet);
	bool MatchesAttributes(const TTuple<FGameplayAttribute, FGameplayAttribute>& Pair) const;

	// GameState처럼 AttributeSet이 아닌 곳에서 받은 HP 값도 같은 위젯 그래프로 표시할 수 있게 한다.
	UFUNCTION(BlueprintCallable, Category = "LB|UI|Attributes")
	void SetAttributeValues(float NewValue, float NewMaxValue);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Attribute Change"))
	void BP_OnAttributeChange(float NewValue, float NewMaxValue);
};
