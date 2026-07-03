// LB_AttributeWidget.cpp

#include "UI/LB_AttributeWidget.h"

void ULB_AttributeWidget::OnAttributeChange(const TTuple<FGameplayAttribute, FGameplayAttribute>& Pair, ULB_AttributeSet* AttributeSet)
{
	const float AttributeValue = Pair.Key.GetNumericValue(AttributeSet);
	const float MaxAttributeValue = Pair.Value.GetNumericValue(AttributeSet);

	SetAttributeValues(AttributeValue, MaxAttributeValue);
}

bool ULB_AttributeWidget::MatchesAttributes(const TTuple<FGameplayAttribute, FGameplayAttribute>& Pair) const
{
	return Attribute == Pair.Key && MaxAttribute == Pair.Value;
}

void ULB_AttributeWidget::SetAttributeValues(float NewValue, float NewMaxValue)
{
	// 월드 위젯과 화면 상단 보스 HP 위젯이 같은 블루프린트 이벤트를 재사용한다.
	BP_OnAttributeChange(NewValue, NewMaxValue);
}
