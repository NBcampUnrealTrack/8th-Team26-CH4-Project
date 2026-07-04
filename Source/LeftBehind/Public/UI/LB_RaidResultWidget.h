// LB_RaidResultWidget.h

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "System/Raid/LBRaidTypes.h"
#include "LB_RaidResultWidget.generated.h"

class STextBlock;
class SWidget;

// 레이드 결과를 화면 중앙에 보여주는 기본 위젯이다.
// 디자이너가 UMG BP를 만들기 전에도 C++ 기본 화면으로 승리 여부를 확인할 수 있다.
UCLASS(Blueprintable)
class LEFTBEHIND_API ULB_RaidResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	ULB_RaidResultWidget(const FObjectInitializer& ObjectInitializer);

	// GameState에서 복제된 최종 결과를 위젯에 넣는다.
	UFUNCTION(BlueprintCallable, Category = "LB|Raid")
	void SetRaidResult(const FLBRaidResultData& InResultData);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	// BP 위젯을 만들었을 때 텍스트, 애니메이션, 버튼 표시를 자유롭게 바꿀 수 있는 확장 지점이다.
	UFUNCTION(BlueprintImplementableEvent, Category = "LB|Raid", meta = (DisplayName = "On Raid Result Changed"))
	void BP_OnRaidResultChanged(const FLBRaidResultData& InResultData);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LB|Raid")
	FText VictoryTitleText;

private:
	FLBRaidResultData CachedResultData;
	TSharedPtr<STextBlock> TitleTextBlock;
	TSharedPtr<STextBlock> DetailTextBlock;

	FText MakeDetailText() const;
	void RefreshText();
};
