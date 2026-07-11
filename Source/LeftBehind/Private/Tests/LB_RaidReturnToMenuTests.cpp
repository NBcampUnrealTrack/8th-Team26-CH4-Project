#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameMode/LB_RaidGameMode.h"
#include "GameState/LB_RaidGameState.h"
#include "Player/LB_PlayerController.h"
#include "TimerManager.h"
#include "UI/HUD/LB_RaidHUDWidget.h"
#include "UI/Popup/LB_RaidResultWidget.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBRaidReturnToMenuContractTest,
	"LeftBehind.Raid.Result.ReturnToMenu",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBRaidReturnToMenuContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const ALB_RaidGameMode* RaidGameModeCDO = GetDefault<ALB_RaidGameMode>();
	TestEqual(
		TEXT("The native return destination is the production main-menu package"),
		RaidGameModeCDO->GetMainMenuMap().ToSoftObjectPath().GetLongPackageName(),
		FString(TEXT("/Game/LeftBehind/Maps/L_MainMenu")));
	TestTrue(
		TEXT("Returning to the main menu preserves the EOS party and lobby selections with seamless travel"),
		RaidGameModeCDO->bUseSeamlessTravel);
	TestTrue(
		TEXT("The native raid game mode always falls back to the raid player controller"),
		RaidGameModeCDO->PlayerControllerClass
			&& RaidGameModeCDO->PlayerControllerClass->IsChildOf(ALB_PlayerController::StaticClass()));
	TestNull(
		TEXT("Remote clients have no ServerReturnToMainMenu RPC surface"),
		ALB_PlayerController::StaticClass()->FindFunctionByName(TEXT("ServerReturnToMainMenu")));

	UWorld* TestWorld = UWorld::CreateWorld(
		EWorldType::Game,
		false,
		TEXT("LBRaidReturnToMenuContractWorld"));
	TestNotNull(TEXT("A transient standalone world can be created for travel policy checks"), TestWorld);
	if (TestWorld)
	{
		ALB_RaidGameMode* TestGameMode = NewObject<ALB_RaidGameMode>(TestWorld->PersistentLevel);
		ALB_RaidGameState* TestGameState = NewObject<ALB_RaidGameState>(TestWorld->PersistentLevel);
		APlayerController* TestController = NewObject<APlayerController>(TestWorld->PersistentLevel);
		ALB_PlayerController* TestRaidController = NewObject<ALB_PlayerController>(TestWorld->PersistentLevel);
		TestNotNull(TEXT("The policy test raid game mode is constructed"), TestGameMode);
		TestNotNull(TEXT("The policy test raid game state is constructed"), TestGameState);
		TestNotNull(TEXT("The policy test local controller is constructed"), TestController);
		TestNotNull(TEXT("The raid HUD retry controller is constructed"), TestRaidController);

		if (TestGameMode && TestGameState && TestController)
		{
			TestGameMode->GameState = TestGameState;
			TestController->SetAsLocalPlayerController();
			TestTrue(TEXT("The policy test game mode has authority"), TestGameMode->HasAuthority());
			TestTrue(TEXT("The policy test controller has authority"), TestController->HasAuthority());
			TestTrue(TEXT("The policy test controller is local"), TestController->IsLocalController());
			TestEqual(TEXT("The policy test world is standalone"), TestGameMode->GetNetMode(), NM_Standalone);
			TestTrue(
				TEXT("The policy test actors share one world"),
				TestController->GetWorld() == TestGameMode->GetWorld());
			TestFalse(
				TEXT("A return request is rejected outside Result"),
				TestGameMode->CanReturnToMainMenu(TestController));

			TestGameState->RaidState = ELBRaidState::Result;
			TestTrue(
				TEXT("A local standalone controller is accepted in Result"),
				TestGameMode->CanReturnToMainMenu(TestController));

			// The real ServerTravel path is covered by Listen PIE. Set the private guard directly
			// here so this unit test never schedules a level change in the automation worker world.
			TestGameMode->bReturnTravelInProgress = true;
			TestFalse(
				TEXT("A second request is blocked while return travel is in progress"),
				TestGameMode->CanReturnToMainMenu(TestController));
			TestFalse(
				TEXT("A duplicate return request cannot start another travel"),
				TestGameMode->TryReturnToMainMenu(TestController));
			TestGameMode->bReturnTravelInProgress = false;
		}

		if (TestRaidController)
		{
			TestRaidController->ScheduleRaidHUDInitializationRetry();
			FTimerManager& TimerManager = TestWorld->GetTimerManager();
			TestTrue(
				TEXT("A missing client dependency starts the low-frequency HUD retry"),
				TimerManager.IsTimerActive(TestRaidController->RaidHUDInitRetryTimerHandle));

			// TimerManager는 새 타이머를 첫 Tick 끝에 활성화하므로 서로 다른 두 프레임을 진행한다.
			// 원격 클라이언트에서 첫 0.1초 안에 복제가 끝나지 않아도 재시도가 사라지면 안 된다.
			++GFrameCounter;
			TimerManager.Tick(0.f);
			++GFrameCounter;
			TimerManager.Tick(0.11f);
			TestTrue(
				TEXT("The HUD retry remains active after its first unsuccessful callback"),
				TimerManager.IsTimerActive(TestRaidController->RaidHUDInitRetryTimerHandle));
			TimerManager.ClearTimer(TestRaidController->RaidHUDInitRetryTimerHandle);
		}

		TestWorld->DestroyWorld(false);
	}

	const ALB_PlayerController* RaidPlayerControllerCDO = GetDefault<ALB_PlayerController>();
	const FString RaidHUDClassPath = RaidPlayerControllerCDO->RaidHUDWidgetClass.ToSoftObjectPath().ToString();
	TestEqual(
		TEXT("The native raid controller keeps the packaged raid HUD fallback"),
		RaidHUDClassPath,
		FString(TEXT("/Game/LeftBehind/UI/BattleHUD/HUD/WBP_LB_RaidHUDWidget.WBP_LB_RaidHUDWidget_C")));
	UClass* RaidHUDClass = LoadClass<ULB_RaidHUDWidget>(nullptr, *RaidHUDClassPath);
	TestNotNull(TEXT("The packaged raid HUD widget class loads"), RaidHUDClass);
	if (RaidHUDClass)
	{
		TestTrue(
			TEXT("The raid HUD asset uses the native raid HUD implementation"),
			RaidHUDClass->IsChildOf(ULB_RaidHUDWidget::StaticClass()));
	}

	const TCHAR* ResultWidgetClassPath =
		TEXT("/Game/LeftBehind/UI/BattleHUD/Popup/WBP_LB_RaidResultWidget.WBP_LB_RaidResultWidget_C");
	UClass* ResultWidgetClass = LoadClass<ULB_RaidResultWidget>(nullptr, ResultWidgetClassPath);
	TestNotNull(TEXT("The packaged raid-result widget class loads"), ResultWidgetClass);
	if (!ResultWidgetClass)
	{
		return false;
	}

	TestTrue(
		TEXT("The raid-result Widget Blueprint directly uses the native raid-result widget parent"),
		ResultWidgetClass->GetSuperClass() == ULB_RaidResultWidget::StaticClass());

	const UWidgetBlueprintGeneratedClass* ResultWidgetGeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(ResultWidgetClass);
	TestNotNull(TEXT("The raid-result asset is a Widget Blueprint generated class"), ResultWidgetGeneratedClass);
	if (!ResultWidgetGeneratedClass)
	{
		return false;
	}

	const UWidgetTree* WidgetTree = ResultWidgetGeneratedClass->GetWidgetTreeArchetype();
	TestNotNull(TEXT("The raid-result Widget Blueprint owns a widget tree"), WidgetTree);
	if (!WidgetTree)
	{
		return false;
	}

	const UPanelWidget* ContentPanel = Cast<UPanelWidget>(WidgetTree->FindWidget(TEXT("VB_Content")));
	TestNotNull(TEXT("The raid-result widget keeps its VB_Content panel contract"), ContentPanel);

	const UWidget* ReturnButton = WidgetTree->FindWidget(TEXT("BTN_ReturnToMainMenu"));
	TestNotNull(TEXT("The raid-result widget contains BTN_ReturnToMainMenu"), ReturnButton);
	if (!ReturnButton)
	{
		return false;
	}

	if (ContentPanel)
	{
		TestTrue(
			TEXT("The return button is placed directly in VB_Content"),
			ReturnButton->GetParent() == ContentPanel);
		TestEqual(
			TEXT("The return button is the final item in VB_Content"),
			ContentPanel->GetChildIndex(ReturnButton),
			ContentPanel->GetChildrenCount() - 1);
	}

	const TCHAR* PrimaryButtonClassPath =
		TEXT("/Game/LeftBehind/UI/SubWidgets/WBP_Common_ButtonPrimary.WBP_Common_ButtonPrimary_C");
	const UClass* PrimaryButtonClass = LoadClass<UWidget>(nullptr, PrimaryButtonClassPath);
	TestNotNull(TEXT("The common Primary button class loads"), PrimaryButtonClass);
	if (PrimaryButtonClass)
	{
		TestTrue(
			TEXT("BTN_ReturnToMainMenu uses the common Primary button contract"),
			ReturnButton->GetClass()->IsChildOf(PrimaryButtonClass));
	}

	const FMulticastDelegateProperty* ClickDelegate =
		FindFProperty<FMulticastDelegateProperty>(ReturnButton->GetClass(), TEXT("OnBTNClicked"));
	if (!ClickDelegate)
	{
		ClickDelegate = FindFProperty<FMulticastDelegateProperty>(ReturnButton->GetClass(), TEXT("OnClicked"));
	}
	TestNotNull(
		TEXT("BTN_ReturnToMainMenu exposes the supported OnBTNClicked or OnClicked delegate"),
		ClickDelegate);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
