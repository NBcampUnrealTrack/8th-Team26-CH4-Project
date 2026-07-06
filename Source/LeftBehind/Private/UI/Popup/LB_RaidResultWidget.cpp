// LB_RaidResultWidget.cpp

#include "UI/Popup/LB_RaidResultWidget.h"

void ULB_RaidResultWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void ULB_RaidResultWidget::BP_OnRaidResultChanged_Implementation(const FLBRaidResultData& ResultData)
{
	RefreshResult(ResultData);
}

void ULB_RaidResultWidget::RefreshResult(const FLBRaidResultData& ResultData)
{
	BP_UpdateResult(ResultData);
}
