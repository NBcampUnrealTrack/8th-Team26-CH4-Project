// LB_RaidResultWidget.cpp

#include "UI/Popup/LB_RaidResultWidget.h"

void ULB_RaidResultWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void ULB_RaidResultWidget::HandleRaidResultChanged(const FLBRaidResultData& ResultData)
{
	BP_UpdateResult(ResultData);
}
