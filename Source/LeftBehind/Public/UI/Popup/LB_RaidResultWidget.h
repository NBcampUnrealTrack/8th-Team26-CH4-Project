// LB_RaidResultWidget.h

#pragma once

#include "CoreMinimal.h"
#include "System/Raid/LBRaidTypes.h"
#include "UI/Base/LB_BaseRaidWidget.h"
#include "LB_RaidResultWidget.generated.h"

// 레이드 종료 결과 팝업
// BaseRaidWidget을 통해 RaidResult 이벤트 받음

UCLASS()
class LEFTBEHIND_API ULB_RaidResultWidget : public ULB_BaseRaidWidget
{
	GENERATED_BODY()

protected:

	virtual void NativeConstruct() override;

	// BaseRaidWidget에서 전달되는 결과 이벤트
	virtual void BP_OnRaidResultChanged_Implementation(const FLBRaidResultData& ResultData) override;

	// 결과 데이터를 BP에 전달
	void RefreshResult(const FLBRaidResultData& ResultData);

	// 결과 UI 전체 갱신
	UFUNCTION(BlueprintImplementableEvent, Category="LB|Raid Result")
	void BP_UpdateResult(const FLBRaidResultData& ResultData);

};
