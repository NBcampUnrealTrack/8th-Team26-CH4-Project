#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Player/LB_MainMenuPlayerController.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "UI/MainMenu/LB_CodenameEntryWidget.h"
#include "UI/MainMenu/LB_MainMenuRootWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"
#include "UnrealEdGlobals.h"

#include "LBMainMenuPIETestBridge.h"

namespace LBMainMenuBackPIETest
{
	constexpr double TimeoutSeconds = 45.0;

	enum class EStage : uint8
	{
		WaitForMainMenu,
		WaitForCodename,
		WaitForRestoredMainMenu,
		WaitForRootMenu,
	};

	struct FState
	{
		TStrongObjectPtr<ULevelEditorPlaySettings> PlaySettings;
		TWeakObjectPtr<ULB_MainMenuRootWidget> OriginalMainMenu;
		EStage Stage = EStage::WaitForMainMenu;
		double Deadline = 0.0;
	};

	ALB_MainMenuPlayerController* FindLocalController(UWorld* World)
	{
		if (!IsValid(World))
		{
			return nullptr;
		}

		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			ALB_MainMenuPlayerController* Controller = Cast<ALB_MainMenuPlayerController>(It->Get());
			if (IsValid(Controller) && Controller->IsLocalController())
			{
				return Controller;
			}
		}
		return nullptr;
	}

	UWorld* FindStandalonePIEWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}

		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			UWorld* World = WorldContext.World();
			if (IsValid(World)
				&& WorldContext.WorldType == EWorldType::PIE
				&& World->GetNetMode() == NM_Standalone)
			{
				return World;
			}
		}
		return nullptr;
	}

	template <typename WidgetType>
	WidgetType* FindVisibleWidget(UWorld* World, const APlayerController* OwningController)
	{
		for (TObjectIterator<WidgetType> It; It; ++It)
		{
			WidgetType* Widget = *It;
			if (IsValid(Widget)
				&& !Widget->HasAnyFlags(RF_ClassDefaultObject)
				&& Widget->GetWorld() == World
				&& Widget->GetOwningPlayer() == OwningController
				&& Widget->IsInViewport())
			{
				return Widget;
			}
		}
		return nullptr;
	}

	class FStartPIE final : public IAutomationLatentCommand
	{
	public:
		FStartPIE(const TSharedRef<FState>& InState, FAutomationTestBase* InTest)
			: State(InState)
			, Test(InTest)
		{
		}

		virtual bool Update() override
		{
			if (!GUnrealEd)
			{
				Test->AddError(TEXT("GUnrealEd is unavailable; standalone PIE cannot be started."));
				return true;
			}
			if (GUnrealEd->PlayWorld)
			{
				Test->AddError(TEXT("Another PIE session is already running."));
				return true;
			}

			ULevelEditorPlaySettings* Settings = DuplicateObject<ULevelEditorPlaySettings>(
				GetDefault<ULevelEditorPlaySettings>(),
				GetTransientPackage());
			State->PlaySettings.Reset(Settings);
			Settings->SetPlayNetMode(PIE_Standalone);
			Settings->SetRunUnderOneProcess(true);
			Settings->SetPlayNumberOfClients(1);
			Settings->bLaunchSeparateServer = false;

			FRequestPlaySessionParams Params;
			Params.SessionDestination = EPlaySessionDestinationType::InProcess;
			Params.WorldType = EPlaySessionWorldType::PlayInEditor;
			Params.EditorPlaySettings = Settings;
			Params.GlobalMapOverride = TEXT("/Game/LeftBehind/Maps/L_MainMenu");
			Params.bAllowOnlineSubsystem = false;
			GUnrealEd->RequestPlaySession(Params);

			State->Deadline = FPlatformTime::Seconds() + TimeoutSeconds;
			return true;
		}

	private:
		TSharedRef<FState> State;
		FAutomationTestBase* Test;
	};

	class FDriveBackFlow final : public IAutomationLatentCommand
	{
	public:
		FDriveBackFlow(const TSharedRef<FState>& InState, FAutomationTestBase* InTest)
			: State(InState)
			, Test(InTest)
		{
		}

		virtual bool Update() override
		{
			if (FPlatformTime::Seconds() > State->Deadline)
			{
				Test->AddError(FString::Printf(
					TEXT("Codename Back PIE flow timed out at stage %d."),
					static_cast<int32>(State->Stage)));
				return true;
			}

			UWorld* World = FindStandalonePIEWorld();
			ALB_MainMenuPlayerController* Controller = FindLocalController(World);
			if (!IsValid(Controller) || !Controller->HasActorBegunPlay())
			{
				return false;
			}

			switch (State->Stage)
			{
			case EStage::WaitForMainMenu:
			{
				ULB_MainMenuRootWidget* MainMenu = FindVisibleWidget<ULB_MainMenuRootWidget>(World, Controller);
				if (!IsValid(MainMenu))
				{
					return false;
				}

				State->OriginalMainMenu = MainMenu;
				Controller->BeginOnlinePlay();
				State->Stage = EStage::WaitForCodename;
				return false;
			}

			case EStage::WaitForCodename:
			{
				ULB_CodenameEntryWidget* Codename = FindVisibleWidget<ULB_CodenameEntryWidget>(World, Controller);
				if (!IsValid(Codename))
				{
					return false;
				}
				if (!ULBMainMenuPIETestBridge::ScheduleWidgetClick(Codename, TEXT("BTN_CodeName_Back")))
				{
					Test->AddError(TEXT("Could not schedule the codename Back button click."));
					return true;
				}
				State->Stage = EStage::WaitForRestoredMainMenu;
				return false;
			}

			case EStage::WaitForRestoredMainMenu:
			{
				ULB_MainMenuRootWidget* MainMenu = FindVisibleWidget<ULB_MainMenuRootWidget>(World, Controller);
				if (!IsValid(MainMenu) || MainMenu == State->OriginalMainMenu.Get())
				{
					return false;
				}

				Test->TestFalse(
					TEXT("The animation-tainted main-menu instance stays detached"),
					State->OriginalMainMenu.IsValid() && State->OriginalMainMenu->IsInViewport());

				UWidget* StartPanel = MainMenu->WidgetTree
					? MainMenu->WidgetTree->FindWidget(TEXT("SB_StartPanel"))
					: nullptr;
				UWidget* RootPanel = MainMenu->WidgetTree
					? MainMenu->WidgetTree->FindWidget(TEXT("SB_RootMenu"))
					: nullptr;
				UWidget* BackButton = MainMenu->WidgetTree
					? MainMenu->WidgetTree->FindWidget(TEXT("BTN_BackBtn"))
					: nullptr;
				Test->TestNotNull(TEXT("The restored main menu contains SB_StartPanel"), StartPanel);
				Test->TestNotNull(TEXT("The restored main menu contains SB_RootMenu"), RootPanel);
				Test->TestNotNull(TEXT("The restored main menu contains BTN_BackBtn"), BackButton);
				if (StartPanel && RootPanel && BackButton)
				{
					Test->TestEqual(
						TEXT("Back restores the room-entry panel visibility"),
						StartPanel->GetVisibility(),
						ESlateVisibility::Visible);
					Test->TestTrue(
						TEXT("The restored room-entry panel is not animation-transparent"),
						StartPanel->GetRenderOpacity() > 0.0f);
					Test->TestEqual(
						TEXT("The root panel remains hidden behind room entry"),
						RootPanel->GetVisibility(),
						ESlateVisibility::Hidden);
					Test->TestEqual(
						TEXT("The room-entry Back button is visible"),
						BackButton->GetVisibility(),
						ESlateVisibility::Visible);
					if (!ULBMainMenuPIETestBridge::ScheduleWidgetClick(MainMenu, TEXT("BTN_BackBtn")))
					{
						Test->AddError(TEXT("Could not schedule the restored room-entry Back button click."));
						return true;
					}
					State->Stage = EStage::WaitForRootMenu;
					return false;
				}
				return true;
			}

			case EStage::WaitForRootMenu:
			{
				ULB_MainMenuRootWidget* MainMenu = FindVisibleWidget<ULB_MainMenuRootWidget>(World, Controller);
				if (!IsValid(MainMenu))
				{
					return false;
				}

				UWidget* StartPanel = MainMenu->WidgetTree
					? MainMenu->WidgetTree->FindWidget(TEXT("SB_StartPanel"))
					: nullptr;
				UWidget* RootPanel = MainMenu->WidgetTree
					? MainMenu->WidgetTree->FindWidget(TEXT("SB_RootMenu"))
					: nullptr;
				UWidget* BackButton = MainMenu->WidgetTree
					? MainMenu->WidgetTree->FindWidget(TEXT("BTN_BackBtn"))
					: nullptr;
				Test->TestNotNull(TEXT("The final main menu contains SB_StartPanel"), StartPanel);
				Test->TestNotNull(TEXT("The final main menu contains SB_RootMenu"), RootPanel);
				Test->TestNotNull(TEXT("The final main menu contains BTN_BackBtn"), BackButton);
				if (StartPanel && RootPanel && BackButton)
				{
					if (StartPanel->GetVisibility() != ESlateVisibility::Hidden
						|| RootPanel->GetVisibility() != ESlateVisibility::Visible
						|| BackButton->GetVisibility() != ESlateVisibility::Hidden)
					{
						return false;
					}
					Test->TestEqual(
						TEXT("Second Back hides the room-entry panel"),
						StartPanel->GetVisibility(),
						ESlateVisibility::Hidden);
					Test->TestEqual(
						TEXT("Second Back restores the root menu"),
						RootPanel->GetVisibility(),
						ESlateVisibility::Visible);
					Test->TestTrue(
						TEXT("The restored root menu is opaque"),
						RootPanel->GetRenderOpacity() > 0.0f);
					Test->TestEqual(
						TEXT("Second Back hides the room-entry Back button"),
						BackButton->GetVisibility(),
						ESlateVisibility::Hidden);
				}
				return true;
			}
			}
			return false;
		}

	private:
		TSharedRef<FState> State;
		FAutomationTestBase* Test;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBMainMenuCodenameBackPIETest,
	"LeftBehind.MainMenu.Codename.BackRestoresRoomEntryPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBMainMenuCodenameBackPIETest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const TSharedRef<LBMainMenuBackPIETest::FState> State =
		MakeShared<LBMainMenuBackPIETest::FState>();
	ADD_LATENT_AUTOMATION_COMMAND(LBMainMenuBackPIETest::FStartPIE(State, this));
	ADD_LATENT_AUTOMATION_COMMAND(LBMainMenuBackPIETest::FDriveBackFlow(State, this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
