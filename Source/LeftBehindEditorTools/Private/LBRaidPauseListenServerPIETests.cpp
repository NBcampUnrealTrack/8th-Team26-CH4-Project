#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameMode/LB_RaidGameMode.h"
#include "GameState/LB_RaidGameState.h"
#include "Player/LB_PlayerController.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "UI/Popup/LB_RaidPauseMenuWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"
#include "UnrealEdGlobals.h"

namespace LBRaidPausePIETest
{
	constexpr double InitialPIETimeoutSeconds = 45.0;
	constexpr double StageTimeoutSeconds = 10.0;
	constexpr double TimeObservationSeconds = 0.25;
	constexpr float MinimumUnpausedAdvanceSeconds = 0.05f;
	constexpr float MaximumPausedAdvanceSeconds = 0.001f;

	enum class EStage : uint8
	{
		WaitForRaid,
		WaitForClientMenu,
		ObserveClientLocalMenu,
		WaitForClientMenuClosed,
		WaitForHostPause,
		ObserveHostPause,
		WaitForClientMenuDuringHostPause,
		WaitForHostNoticeRestored,
		WaitForCleanup,
		ObserveCleanup,
	};

	struct FState
	{
		TStrongObjectPtr<ULevelEditorPlaySettings> PlaySettings;
		EStage Stage = EStage::WaitForRaid;
		double StageDeadline = 0.0;
		double ObservationDeadline = 0.0;
		float SampledServerWorldTime = 0.0f;
		FName OriginalGameNetDriverClass;
		FName OriginalGameNetDriverFallbackClass;
		bool bGameNetDriverOverrideActive = false;
	};

	bool OverrideGameNetDriverForPIE(const TSharedRef<FState>& State)
	{
		if (!GEngine)
		{
			return false;
		}

		for (FNetDriverDefinition& Definition : GEngine->NetDriverDefinitions)
		{
			if (Definition.DefName == NAME_GameNetDriver)
			{
				State->OriginalGameNetDriverClass = Definition.DriverClassName;
				State->OriginalGameNetDriverFallbackClass = Definition.DriverClassNameFallback;
				Definition.DriverClassName = FName(TEXT("/Script/OnlineSubsystemUtils.IpNetDriver"));
				Definition.DriverClassNameFallback = Definition.DriverClassName;
				State->bGameNetDriverOverrideActive = true;
				return true;
			}
		}

		return false;
	}

	void RestoreGameNetDriverAfterPIE(const TSharedRef<FState>& State)
	{
		if (!State->bGameNetDriverOverrideActive || !GEngine)
		{
			return;
		}

		for (FNetDriverDefinition& Definition : GEngine->NetDriverDefinitions)
		{
			if (Definition.DefName == NAME_GameNetDriver)
			{
				Definition.DriverClassName = State->OriginalGameNetDriverClass;
				Definition.DriverClassNameFallback = State->OriginalGameNetDriverFallbackClass;
				break;
			}
		}

		State->bGameNetDriverOverrideActive = false;
	}

	void AdvanceStage(const TSharedRef<FState>& State, const EStage NewStage)
	{
		State->Stage = NewStage;
		State->StageDeadline = FPlatformTime::Seconds() + StageTimeoutSeconds;
	}

	bool IsRaidWorld(const UWorld* World)
	{
		return IsValid(World) && World->GetMapName().EndsWith(TEXT("_Main"));
	}

	void FindNetworkWorlds(UWorld*& OutListenServerWorld, UWorld*& OutClientWorld)
	{
		OutListenServerWorld = nullptr;
		OutClientWorld = nullptr;

		if (!GEngine)
		{
			return;
		}

		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			UWorld* World = WorldContext.World();
			if (!IsValid(World) || WorldContext.WorldType != EWorldType::PIE)
			{
				continue;
			}

			if (World->GetNetMode() == NM_ListenServer)
			{
				OutListenServerWorld = World;
			}
			else if (World->GetNetMode() == NM_Client)
			{
				OutClientWorld = World;
			}
		}
	}

	ALB_PlayerController* FindLocalRaidController(UWorld* World)
	{
		if (!IsValid(World))
		{
			return nullptr;
		}

		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			ALB_PlayerController* Controller = Cast<ALB_PlayerController>(It->Get());
			if (IsValid(Controller) && Controller->IsLocalController())
			{
				return Controller;
			}
		}

		return nullptr;
	}

	ULB_RaidPauseMenuWidget* FindPauseWidget(
		UWorld* World,
		const ALB_PlayerController* OwningController)
	{
		if (!IsValid(World) || !IsValid(OwningController))
		{
			return nullptr;
		}

		for (TObjectIterator<ULB_RaidPauseMenuWidget> It; It; ++It)
		{
			ULB_RaidPauseMenuWidget* Widget = *It;
			if (IsValid(Widget)
				&& !Widget->HasAnyFlags(RF_ClassDefaultObject)
				&& Widget->GetWorld() == World
				&& Widget->GetOwningPlayer() == OwningController)
			{
				return Widget;
			}
		}

		return nullptr;
	}

	bool IsPresentationVisible(
		const ULB_RaidPauseMenuWidget* Widget,
		const FName VisibleContractWidget,
		const FName CollapsedContractWidget)
	{
		if (!IsValid(Widget) || !Widget->IsVisible() || !Widget->WidgetTree)
		{
			return false;
		}

		const UWidget* VisibleWidget = Widget->WidgetTree->FindWidget(VisibleContractWidget);
		const UWidget* CollapsedWidget = Widget->WidgetTree->FindWidget(CollapsedContractWidget);
		return IsValid(VisibleWidget)
			&& VisibleWidget->GetVisibility() == ESlateVisibility::Visible
			&& IsValid(CollapsedWidget)
			&& CollapsedWidget->GetVisibility() == ESlateVisibility::Collapsed;
	}

	bool IsActionMenuVisible(const ULB_RaidPauseMenuWidget* Widget)
	{
		return IsPresentationVisible(Widget, TEXT("PNL_ActionMenu"), TEXT("TXT_HostPauseNotice"));
	}

	bool IsHostPauseNoticeVisible(const ULB_RaidPauseMenuWidget* Widget)
	{
		return IsPresentationVisible(Widget, TEXT("TXT_HostPauseNotice"), TEXT("PNL_ActionMenu"));
	}

	void CleanupPauseBestEffort(UWorld* ListenServerWorld, UWorld* ClientWorld)
	{
		ALB_PlayerController* ClientController = FindLocalRaidController(ClientWorld);
		if (IsValid(ClientController) && ClientController->IsPauseMenuOpen())
		{
			ClientController->ClosePauseMenu();
		}

		ALB_PlayerController* HostController = FindLocalRaidController(ListenServerWorld);
		if (IsValid(HostController) && HostController->IsPauseMenuOpen())
		{
			HostController->ClosePauseMenu();
		}

		if (IsValid(HostController) && HostController->IsPaused())
		{
			if (ALB_RaidGameMode* RaidGameMode = ListenServerWorld->GetAuthGameMode<ALB_RaidGameMode>())
			{
				RaidGameMode->TrySetHostPause(HostController, false);
			}
		}
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
				Test->AddError(TEXT("GUnrealEd is unavailable; the two-player pause PIE test cannot start."));
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
			Settings->SetPlayNetMode(PIE_ListenServer);
			Settings->SetRunUnderOneProcess(true);
			Settings->SetPlayNumberOfClients(2);
			Settings->SetServerPort(19078);
			Settings->bLaunchSeparateServer = false;

			if (!OverrideGameNetDriverForPIE(State))
			{
				Test->AddError(TEXT("GameNetDriver was unavailable for the test-only IP override."));
				return true;
			}

			FRequestPlaySessionParams Params;
			Params.SessionDestination = EPlaySessionDestinationType::InProcess;
			Params.WorldType = EPlaySessionWorldType::PlayInEditor;
			Params.EditorPlaySettings = Settings;
			Params.GlobalMapOverride = TEXT("/Game/LeftBehind/Maps/Main");
			Params.bAllowOnlineSubsystem = false;
			GUnrealEd->RequestPlaySession(Params);

			State->StageDeadline = FPlatformTime::Seconds() + InitialPIETimeoutSeconds;
			return true;
		}

	private:
		TSharedRef<FState> State;
		FAutomationTestBase* Test;
	};

	class FDrivePIE final : public IAutomationLatentCommand
	{
	public:
		FDrivePIE(const TSharedRef<FState>& InState, FAutomationTestBase* InTest)
			: State(InState)
			, Test(InTest)
		{
		}

		virtual bool Update() override
		{
			UWorld* ListenServerWorld = nullptr;
			UWorld* ClientWorld = nullptr;
			FindNetworkWorlds(ListenServerWorld, ClientWorld);
			// Both drivers have already been instantiated. Restore the project-wide
			// EOS definition immediately so subsequent editor work is unaffected.
			if (IsValid(ListenServerWorld) && IsValid(ClientWorld))
			{
				RestoreGameNetDriverAfterPIE(State);
			}

			if (FPlatformTime::Seconds() > State->StageDeadline)
			{
				Test->AddError(FString::Printf(
					TEXT("Two-player raid pause PIE timed out at stage %d. ServerWorld=%s ClientWorld=%s"),
					static_cast<int32>(State->Stage),
					*GetNameSafe(ListenServerWorld),
					*GetNameSafe(ClientWorld)));
				RestoreGameNetDriverAfterPIE(State);
				CleanupPauseBestEffort(ListenServerWorld, ClientWorld);
				return true;
			}

			ALB_PlayerController* HostController = FindLocalRaidController(ListenServerWorld);
			ALB_PlayerController* ClientController = FindLocalRaidController(ClientWorld);
			ALB_RaidGameState* ServerGameState = IsValid(ListenServerWorld)
				? ListenServerWorld->GetGameState<ALB_RaidGameState>()
				: nullptr;
			ALB_RaidGameState* ClientGameState = IsValid(ClientWorld)
				? ClientWorld->GetGameState<ALB_RaidGameState>()
				: nullptr;
			ULB_RaidPauseMenuWidget* HostPauseWidget = FindPauseWidget(ListenServerWorld, HostController);
			ULB_RaidPauseMenuWidget* ClientPauseWidget = FindPauseWidget(ClientWorld, ClientController);

			switch (State->Stage)
			{
			case EStage::WaitForRaid:
				if (!IsRaidWorld(ListenServerWorld)
					|| !IsRaidWorld(ClientWorld)
					|| !IsValid(HostController)
					|| !IsValid(ClientController)
					|| !IsValid(ServerGameState)
					|| !IsValid(ClientGameState)
					|| !IsValid(HostPauseWidget)
					|| !IsValid(ClientPauseWidget)
					|| ServerGameState->PlayerArray.Num() != 2)
				{
					return false;
				}

				Test->TestTrue(TEXT("The PIE host uses the local listen-host path"), HostController->IsLocalListenHost());
				Test->TestFalse(TEXT("The remote participant is not a local listen host"), ClientController->IsLocalListenHost());
				ClientController->TogglePauseMenu();
				AdvanceStage(State, EStage::WaitForClientMenu);
				return false;

			case EStage::WaitForClientMenu:
				if (!IsValid(HostController)
					|| !IsValid(ClientController)
					|| !IsValid(ServerGameState)
					|| !ClientController->IsPauseMenuOpen()
					|| !IsActionMenuVisible(ClientPauseWidget))
				{
					return false;
				}

				Test->TestFalse(TEXT("A participant menu does not set replicated host pause"), ServerGameState->bHostPauseActive);
				Test->TestFalse(TEXT("A participant menu does not pause the listen server"), HostController->IsPaused());
				Test->TestFalse(TEXT("A participant menu does not pause its client world"), ClientController->IsPaused());
				Test->TestFalse(TEXT("A participant menu does not open the host menu"), HostController->IsPauseMenuOpen());
				Test->TestTrue(TEXT("The participant menu enables its local cursor"), ClientController->bShowMouseCursor);
				State->SampledServerWorldTime = ListenServerWorld->GetTimeSeconds();
				State->ObservationDeadline = FPlatformTime::Seconds() + TimeObservationSeconds;
				AdvanceStage(State, EStage::ObserveClientLocalMenu);
				return false;

			case EStage::ObserveClientLocalMenu:
				if (FPlatformTime::Seconds() < State->ObservationDeadline)
				{
					return false;
				}

				Test->TestTrue(
					TEXT("Server game time advances while only the participant menu is open"),
					IsValid(ListenServerWorld)
						&& ListenServerWorld->GetTimeSeconds() - State->SampledServerWorldTime
							>= MinimumUnpausedAdvanceSeconds);
				if (!IsValid(ClientController))
				{
					Test->AddError(TEXT("The participant controller disappeared while its menu was open."));
					CleanupPauseBestEffort(ListenServerWorld, ClientWorld);
					return true;
				}
				ClientController->TogglePauseMenu();
				AdvanceStage(State, EStage::WaitForClientMenuClosed);
				return false;

			case EStage::WaitForClientMenuClosed:
				if (!IsValid(HostController)
					|| !IsValid(ClientController)
					|| ClientController->IsPauseMenuOpen())
				{
					return false;
				}

				Test->TestFalse(TEXT("Closing the participant menu restores its local cursor"), ClientController->bShowMouseCursor);
				HostController->TogglePauseMenu();
				AdvanceStage(State, EStage::WaitForHostPause);
				return false;

			case EStage::WaitForHostPause:
				if (!IsValid(HostController)
					|| !IsValid(ClientController)
					|| !IsValid(ServerGameState)
					|| !IsValid(ClientGameState)
					|| !HostController->IsPauseMenuOpen()
					|| !HostController->IsPaused()
					|| !ClientController->IsPaused()
					|| !ServerGameState->bHostPauseActive
					|| !ClientGameState->bHostPauseActive
					|| !IsActionMenuVisible(HostPauseWidget)
					|| !IsHostPauseNoticeVisible(ClientPauseWidget))
				{
					return false;
				}

				Test->TestTrue(TEXT("The replicated host pause also pauses the participant world"), ClientController->IsPaused());
				Test->TestFalse(TEXT("The participant notice is not a local action menu"), ClientController->IsPauseMenuOpen());
				State->SampledServerWorldTime = ListenServerWorld->GetTimeSeconds();
				State->ObservationDeadline = FPlatformTime::Seconds() + TimeObservationSeconds;
				AdvanceStage(State, EStage::ObserveHostPause);
				return false;

			case EStage::ObserveHostPause:
				if (FPlatformTime::Seconds() < State->ObservationDeadline)
				{
					return false;
				}

				Test->TestTrue(
					TEXT("Listen-server game time is frozen while the host menu owns pause"),
					IsValid(ListenServerWorld)
						&& FMath::Abs(ListenServerWorld->GetTimeSeconds() - State->SampledServerWorldTime)
							<= MaximumPausedAdvanceSeconds);
				if (!IsValid(ClientController))
				{
					Test->AddError(TEXT("The participant controller disappeared during host pause."));
					CleanupPauseBestEffort(ListenServerWorld, ClientWorld);
					return true;
				}
				// This public handler is the target of the already contract-tested Escape binding.
				// Invoking it while paused verifies the local overlay priority independently of viewport focus.
				ClientController->TogglePauseMenu();
				AdvanceStage(State, EStage::WaitForClientMenuDuringHostPause);
				return false;

			case EStage::WaitForClientMenuDuringHostPause:
				if (!IsValid(ClientController)
					|| !IsValid(ServerGameState)
					|| !ClientController->IsPauseMenuOpen()
					|| !IsActionMenuVisible(ClientPauseWidget))
				{
					return false;
				}

				Test->TestTrue(TEXT("Opening the participant menu preserves the host's global pause"), ServerGameState->bHostPauseActive);
				ClientController->TogglePauseMenu();
				AdvanceStage(State, EStage::WaitForHostNoticeRestored);
				return false;

			case EStage::WaitForHostNoticeRestored:
				if (!IsValid(HostController)
					|| !IsValid(ClientController)
					|| ClientController->IsPauseMenuOpen()
					|| !IsHostPauseNoticeVisible(ClientPauseWidget))
				{
					return false;
				}

				HostController->TogglePauseMenu();
				AdvanceStage(State, EStage::WaitForCleanup);
				return false;

			case EStage::WaitForCleanup:
				if (!IsValid(HostController)
					|| !IsValid(ClientController)
					|| !IsValid(ServerGameState)
					|| !IsValid(ClientGameState)
					|| HostController->IsPauseMenuOpen()
					|| ClientController->IsPauseMenuOpen()
					|| HostController->IsPaused()
					|| ClientController->IsPaused()
					|| ServerGameState->bHostPauseActive
					|| ClientGameState->bHostPauseActive
					|| (IsValid(ClientPauseWidget) && ClientPauseWidget->IsVisible()))
				{
					return false;
				}

				Test->TestFalse(TEXT("Host cleanup restores the host cursor"), HostController->bShowMouseCursor);
				Test->TestFalse(TEXT("Host cleanup leaves the participant cursor in gameplay mode"), ClientController->bShowMouseCursor);
				State->SampledServerWorldTime = ListenServerWorld->GetTimeSeconds();
				State->ObservationDeadline = FPlatformTime::Seconds() + TimeObservationSeconds;
				AdvanceStage(State, EStage::ObserveCleanup);
				return false;

			case EStage::ObserveCleanup:
				if (FPlatformTime::Seconds() < State->ObservationDeadline)
				{
					return false;
				}

				Test->TestTrue(
					TEXT("Server game time resumes after the host closes the menu"),
					IsValid(ListenServerWorld)
						&& ListenServerWorld->GetTimeSeconds() - State->SampledServerWorldTime
							>= MinimumUnpausedAdvanceSeconds);
				return true;
			}

			return false;
		}

	private:
		TSharedRef<FState> State;
		FAutomationTestBase* Test;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBRaidPauseListenServerPIETest,
	"LeftBehind.Raid.PauseMenu.ListenServerPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBRaidPauseListenServerPIETest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const TSharedRef<LBRaidPausePIETest::FState> State = MakeShared<LBRaidPausePIETest::FState>();
	ADD_LATENT_AUTOMATION_COMMAND(LBRaidPausePIETest::FStartPIE(State, this));
	ADD_LATENT_AUTOMATION_COMMAND(LBRaidPausePIETest::FDrivePIE(State, this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
