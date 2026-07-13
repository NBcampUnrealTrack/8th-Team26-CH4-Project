#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"

#include "AbilitySystem/LB_AttributeSet.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Player/LB_MainMenuPlayerController.h"
#include "Player/LB_PlayerState.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "UI/Panels/LB_PartyMemberSlotWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"
#include "UnrealEdGlobals.h"

#include "LBMainMenuPIETestBridge.h"

namespace
{
	constexpr double LBPIETimeoutSeconds = 90.0;

	enum class ELBPIEPartyHPStage : uint8
	{
		WaitForLobby,
		WaitForCodenames,
		WaitForRaid,
	};

	struct FLBPIEPartyHPState
	{
		TStrongObjectPtr<ULevelEditorPlaySettings> PlaySettings;
		ELBPIEPartyHPStage Stage = ELBPIEPartyHPStage::WaitForLobby;
		double Deadline = 0.0;
	};

	void FindNetworkPIEWorlds(UWorld*& OutListenServerWorld, UWorld*& OutClientWorld)
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

	ALB_MainMenuPlayerController* FindLocalMenuController(UWorld* World)
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

	bool HasTwoConfirmedPlayers(UWorld* ListenServerWorld)
	{
		const AGameStateBase* GameState = IsValid(ListenServerWorld)
			? ListenServerWorld->GetGameState()
			: nullptr;
		if (!IsValid(GameState) || GameState->PlayerArray.Num() != 2)
		{
			return false;
		}

		for (const APlayerState* PlayerState : GameState->PlayerArray)
		{
			const ALB_PlayerState* LBPlayerState = Cast<ALB_PlayerState>(PlayerState);
			if (!IsValid(LBPlayerState) || !LBPlayerState->IsCodenameConfirmed())
			{
				return false;
			}
		}

		return true;
	}

	bool IsRaidWorld(const UWorld* World)
	{
		return IsValid(World) && World->GetMapName().EndsWith(TEXT("_Main"));
	}

	float ReadAttributeCurrentValue(const ULB_AttributeSet* Attributes, FName AttributeName)
	{
		const FStructProperty* AttributeProperty = FindFProperty<FStructProperty>(
			ULB_AttributeSet::StaticClass(),
			AttributeName);
		const FFloatProperty* CurrentValueProperty = AttributeProperty
			? FindFProperty<FFloatProperty>(AttributeProperty->Struct, TEXT("CurrentValue"))
			: nullptr;
		const void* AttributeData = AttributeProperty && IsValid(Attributes)
			? AttributeProperty->ContainerPtrToValuePtr<void>(Attributes)
			: nullptr;
		return CurrentValueProperty && AttributeData
			? CurrentValueProperty->GetPropertyValue_InContainer(AttributeData)
			: 0.0f;
	}

	class FStartLBListenServerPIE final : public IAutomationLatentCommand
	{
	public:
		FStartLBListenServerPIE(
			const TSharedRef<FLBPIEPartyHPState>& InState,
			FAutomationTestBase* InTest)
			: State(InState)
			, Test(InTest)
		{
		}

		virtual bool Update() override
		{
			if (!GUnrealEd)
			{
				Test->AddError(TEXT("GUnrealEd is unavailable; a two-player PIE session cannot be started."));
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
			Settings->SetServerPort(19077);
			Settings->bLaunchSeparateServer = false;

			FRequestPlaySessionParams Params;
			Params.SessionDestination = EPlaySessionDestinationType::InProcess;
			Params.WorldType = EPlaySessionWorldType::PlayInEditor;
			Params.EditorPlaySettings = Settings;
			Params.GlobalMapOverride = TEXT("/Game/LeftBehind/Maps/L_MainMenu");
			Params.bAllowOnlineSubsystem = false;
			GUnrealEd->RequestPlaySession(Params);

			State->Deadline = FPlatformTime::Seconds() + LBPIETimeoutSeconds;
			return true;
		}

	private:
		TSharedRef<FLBPIEPartyHPState> State;
		FAutomationTestBase* Test;
	};

	class FDriveLBListenServerPartyHPPIE final : public IAutomationLatentCommand
	{
	public:
		FDriveLBListenServerPartyHPPIE(
			const TSharedRef<FLBPIEPartyHPState>& InState,
			FAutomationTestBase* InTest)
			: State(InState)
			, Test(InTest)
		{
		}

		virtual bool Update() override
		{
			if (FPlatformTime::Seconds() > State->Deadline)
			{
				Test->AddError(FString::Printf(
					TEXT("Two-player listen-server PIE timed out at stage %d."),
					static_cast<int32>(State->Stage)));
				return true;
			}

			UWorld* ListenServerWorld = nullptr;
			UWorld* ClientWorld = nullptr;
			FindNetworkPIEWorlds(ListenServerWorld, ClientWorld);

			switch (State->Stage)
			{
			case ELBPIEPartyHPStage::WaitForLobby:
			{
				ALB_MainMenuPlayerController* HostController = FindLocalMenuController(ListenServerWorld);
				ALB_MainMenuPlayerController* ClientController = FindLocalMenuController(ClientWorld);
				const AGameStateBase* ServerGameState = IsValid(ListenServerWorld)
					? ListenServerWorld->GetGameState()
					: nullptr;
				if (!IsValid(HostController)
					|| !IsValid(ClientController)
					|| !IsValid(ServerGameState)
					|| ServerGameState->PlayerArray.Num() != 2)
				{
					return false;
				}

				if (!ULBMainMenuPIETestBridge::ScheduleCodenameSubmit(HostController, TEXT("HostBot"))
					|| !ULBMainMenuPIETestBridge::ScheduleCodenameSubmit(ClientController, TEXT("ClientBot")))
				{
					Test->AddError(TEXT("The PIE bridge could not schedule both codename submissions."));
					return true;
				}

				State->Stage = ELBPIEPartyHPStage::WaitForCodenames;
				return false;
			}

			case ELBPIEPartyHPStage::WaitForCodenames:
			{
				ALB_MainMenuPlayerController* HostController = FindLocalMenuController(ListenServerWorld);
				if (!IsValid(HostController)
					|| !HasTwoConfirmedPlayers(ListenServerWorld)
					|| !HostController->CanRequestStartHunt())
				{
					return false;
				}

				if (!ULBMainMenuPIETestBridge::ScheduleStartRequests(HostController))
				{
					Test->AddError(TEXT("The PIE bridge could not schedule listen-host raid travel."));
					return true;
				}

				State->Stage = ELBPIEPartyHPStage::WaitForRaid;
				return false;
			}

			case ELBPIEPartyHPStage::WaitForRaid:
				return VerifyRaidPartyHP(ListenServerWorld, ClientWorld);
			}

			return false;
		}

	private:
		bool VerifyRaidPartyHP(UWorld* ListenServerWorld, UWorld* ClientWorld)
		{
			if (!IsRaidWorld(ListenServerWorld) || !IsRaidWorld(ClientWorld))
			{
				return false;
			}

			const AGameStateBase* ServerGameState = ListenServerWorld->GetGameState();
			if (!IsValid(ServerGameState) || ServerGameState->PlayerArray.Num() != 2)
			{
				return false;
			}

			ALB_PlayerState* RemotePlayerState = nullptr;
			for (FConstPlayerControllerIterator It = ListenServerWorld->GetPlayerControllerIterator(); It; ++It)
			{
				const APlayerController* Controller = It->Get();
				if (IsValid(Controller) && !Controller->IsLocalController())
				{
					RemotePlayerState = Controller->GetPlayerState<ALB_PlayerState>();
					break;
				}
			}

			if (!IsValid(RemotePlayerState))
			{
				return false;
			}

			for (APlayerState* PlayerState : ServerGameState->PlayerArray)
			{
				const ALB_PlayerState* LBPlayerState = Cast<ALB_PlayerState>(PlayerState);
				const ULB_AttributeSet* Attributes = IsValid(LBPlayerState)
					? LBPlayerState->GetLBAttributeSet()
					: nullptr;
				if (!IsValid(Attributes)
					|| !Attributes->bAttributeInitialized
					|| ReadAttributeCurrentValue(Attributes, TEXT("MaxHealth")) <= 0.0f
					|| ReadAttributeCurrentValue(Attributes, TEXT("Health")) <= 0.0f)
				{
					return false;
				}
			}

			FObjectPropertyBase* CachedPlayerStateProperty = FindFProperty<FObjectPropertyBase>(
				ULB_PartyMemberSlotWidget::StaticClass(),
				TEXT("CachedPlayerState"));
			if (!CachedPlayerStateProperty)
			{
				Test->AddError(TEXT("Party slot no longer exposes its cached PlayerState to reflection."));
				return true;
			}

			ULB_PartyMemberSlotWidget* RemotePartySlot = nullptr;
			for (TObjectIterator<ULB_PartyMemberSlotWidget> It; It; ++It)
			{
				ULB_PartyMemberSlotWidget* PartySlot = *It;
				if (!IsValid(PartySlot)
					|| PartySlot->HasAnyFlags(RF_ClassDefaultObject)
					|| PartySlot->GetWorld() != ListenServerWorld)
				{
					continue;
				}

				ALB_PlayerState* CachedPlayerState = Cast<ALB_PlayerState>(
					CachedPlayerStateProperty->GetObjectPropertyValue_InContainer(PartySlot));
				if (CachedPlayerState == RemotePlayerState)
				{
					RemotePartySlot = PartySlot;
					break;
				}
			}

			if (!IsValid(RemotePartySlot))
			{
				return false;
			}

			ULB_AttributeSet* Attributes = RemotePlayerState->GetLBAttributeSet();
			if (!IsValid(Attributes))
			{
				return false;
			}

			Test->TestTrue(
				TEXT("Listen host sees the remote player's initialized MaxHealth"),
				Attributes->bAttributeInitialized
					&& ReadAttributeCurrentValue(Attributes, TEXT("MaxHealth")) > 0.0f);
			Test->TestTrue(
				TEXT("Listen host party slot observes final attribute initialization"),
				Attributes->OnAttributesInitialized.Contains(
					RemotePartySlot,
					TEXT("OnAttributesInitialized")));
			return true;
		}

		TSharedRef<FLBPIEPartyHPState> State;
		FAutomationTestBase* Test;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBListenServerPartyHPPIETest,
	"LeftBehind.Raid.Party.ListenServerRemoteHPPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBListenServerPartyHPPIETest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const TSharedRef<FLBPIEPartyHPState> State = MakeShared<FLBPIEPartyHPState>();
	ADD_LATENT_AUTOMATION_COMMAND(FStartLBListenServerPIE(State, this));
	ADD_LATENT_AUTOMATION_COMMAND(FDriveLBListenServerPartyHPPIE(State, this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
