// LB_RaidResultWidget.cpp

#include "UI/LB_RaidResultWidget.h"

#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

ULB_RaidResultWidget::ULB_RaidResultWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	VictoryTitleText = FText::FromString(TEXT("VICTORY"));
}

void ULB_RaidResultWidget::SetRaidResult(const FLBRaidResultData& InResultData)
{
	CachedResultData = InResultData;
	RefreshText();
	BP_OnRaidResultChanged(CachedResultData);
}

TSharedRef<SWidget> ULB_RaidResultWidget::RebuildWidget()
{
	// BP 없이도 바로 테스트할 수 있는 기본 Slate 화면이다.
	// 실제 프로젝트 UI가 생기면 이 클래스를 부모로 하는 UMG BP로 교체하면 된다.
	TSharedRef<SWidget> ResultWidget =
		SNew(SOverlay)
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox")))
				.BorderBackgroundColor(FLinearColor(0.02f, 0.04f, 0.06f, 0.88f))
				.Padding(FMargin(56.f, 36.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					.Padding(FMargin(0.f, 0.f, 0.f, 12.f))
					[
						SAssignNew(TitleTextBlock, STextBlock)
						.Text(VictoryTitleText)
						.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 64))
						.ColorAndOpacity(FLinearColor(0.2f, 0.95f, 1.f, 1.f))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					[
						SAssignNew(DetailTextBlock, STextBlock)
						.Text(MakeDetailText())
						.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 24))
						.ColorAndOpacity(FLinearColor::White)
					]
				]
			]
		];

	RefreshText();
	return ResultWidget;
}

FText ULB_RaidResultWidget::MakeDetailText() const
{
	const FString RankText = CachedResultData.RankID.IsNone()
		? TEXT("-")
		: CachedResultData.RankID.ToString();

	return FText::FromString(
		FString::Printf(
			TEXT("Boss defeated  |  Time %.1fs  |  Rank %s  |  Deaths %d"),
			CachedResultData.ClearTimeSec,
			*RankText,
			CachedResultData.PlayerDeaths
		)
	);
}

void ULB_RaidResultWidget::RefreshText()
{
	if (TitleTextBlock.IsValid())
	{
		TitleTextBlock->SetText(VictoryTitleText);
	}

	if (DetailTextBlock.IsValid())
	{
		DetailTextBlock->SetText(MakeDetailText());
	}
}
