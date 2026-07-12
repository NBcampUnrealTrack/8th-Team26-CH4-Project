#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/InputComponent.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameMode/LB_RaidGameMode.h"
#include "GameState/LB_RaidGameState.h"
#include "InputCoreTypes.h"
#include "Net/UnrealNetwork.h"
#include "Player/LB_PlayerController.h"
#include "UI/Popup/LB_RaidPauseMenuWidget.h"
#include "UObject/UnrealType.h"

namespace
{
	constexpr const TCHAR* LBRaidPauseMenuClassPath =
		TEXT("/Game/LeftBehind/UI/BattleHUD/Popup/WBP_LB_RaidPauseMenuWidget.WBP_LB_RaidPauseMenuWidget_C");
	constexpr const TCHAR* LBRaidPausePrimaryButtonClassPath =
		TEXT("/Game/LeftBehind/UI/SubWidgets/WBP_Common_ButtonPrimary.WBP_Common_ButtonPrimary_C");

	const FMulticastDelegateProperty* FindRaidPauseSupportedClickDelegate(const UClass* WidgetClass)
	{
		if (!WidgetClass)
		{
			return nullptr;
		}

		if (const FMulticastDelegateProperty* ClickDelegate =
			FindFProperty<FMulticastDelegateProperty>(WidgetClass, TEXT("OnBTNClicked")))
		{
			return ClickDelegate;
		}

		return FindFProperty<FMulticastDelegateProperty>(WidgetClass, TEXT("OnClicked"));
	}

	const FTextProperty* FindRaidPauseCommonButtonLabelProperty(const UClass* WidgetClass)
	{
		if (!WidgetClass)
		{
			return nullptr;
		}

		if (const FTextProperty* DisplayLabel =
			FindFProperty<FTextProperty>(WidgetClass, TEXT("DisplayLabel")))
		{
			return DisplayLabel;
		}
		if (const FTextProperty* Label = FindFProperty<FTextProperty>(WidgetClass, TEXT("Label")))
		{
			return Label;
		}

		for (TFieldIterator<FTextProperty> PropertyIt(WidgetClass); PropertyIt; ++PropertyIt)
		{
			if (PropertyIt->GetName().Contains(TEXT("Label"), ESearchCase::IgnoreCase))
			{
				return *PropertyIt;
			}
		}

		return nullptr;
	}

	bool IsRaidPauseDescendantOf(const UWidget* Widget, const UWidget* ExpectedAncestor)
	{
		for (const UWidget* Parent = Widget ? Widget->GetParent() : nullptr;
			Parent;
			Parent = Parent->GetParent())
		{
			if (Parent == ExpectedAncestor)
			{
				return true;
			}
		}

		return false;
	}

	void TestRaidPauseContractButton(
		FAutomationTestBase& Test,
		const UWidgetTree* WidgetTree,
		const UClass* PrimaryButtonClass,
		const FName ButtonName,
		const UWidget* ExpectedPanel,
		const TCHAR* ExpectedLabel)
	{
		const FString ButtonLabel = ButtonName.ToString();
		const UWidget* Button = WidgetTree ? WidgetTree->FindWidget(ButtonName) : nullptr;
		Test.TestNotNull(*FString::Printf(TEXT("The pause menu contains %s"), *ButtonLabel), Button);
		if (!Button)
		{
			return;
		}

		if (PrimaryButtonClass)
		{
			Test.TestTrue(
				*FString::Printf(TEXT("%s uses the common Primary button class"), *ButtonLabel),
				Button->GetClass()->IsChildOf(PrimaryButtonClass));
		}

		Test.TestNotNull(
			*FString::Printf(TEXT("%s exposes OnBTNClicked or OnClicked"), *ButtonLabel),
			FindRaidPauseSupportedClickDelegate(Button->GetClass()));

		const FTextProperty* LabelProperty =
			FindRaidPauseCommonButtonLabelProperty(Button->GetClass());
		Test.TestNotNull(
			*FString::Printf(TEXT("%s exposes its common-button label"), *ButtonLabel),
			LabelProperty);
		if (LabelProperty)
		{
			Test.TestEqual(
				*FString::Printf(TEXT("%s has its Korean action label"), *ButtonLabel),
				LabelProperty->GetPropertyValue_InContainer(Button).ToString(),
				FString(ExpectedLabel));
		}

		if (ExpectedPanel)
		{
			Test.TestTrue(
				*FString::Printf(TEXT("%s is inside its intended presentation panel"), *ButtonLabel),
				IsRaidPauseDescendantOf(Button, ExpectedPanel));
		}
	}

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBRaidPauseMenuContractTest,
	"LeftBehind.Raid.PauseMenu.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBRaidPauseMenuContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const ALB_PlayerController* ControllerCDO = GetDefault<ALB_PlayerController>();
	TestEqual(
		TEXT("The native controller keeps the packaged pause-menu fallback"),
		ControllerCDO->RaidPauseMenuWidgetClass.ToSoftObjectPath().ToString(),
		FString(LBRaidPauseMenuClassPath));

	const UClass* PauseMenuClass = LoadClass<ULB_RaidPauseMenuWidget>(nullptr, LBRaidPauseMenuClassPath);
	TestNotNull(TEXT("The packaged raid pause-menu class loads"), PauseMenuClass);
	if (PauseMenuClass)
	{
		TestTrue(
			TEXT("The pause-menu Widget Blueprint directly uses the native pause-menu parent"),
			PauseMenuClass->GetSuperClass() == ULB_RaidPauseMenuWidget::StaticClass());

		const UWidgetBlueprintGeneratedClass* GeneratedClass = Cast<UWidgetBlueprintGeneratedClass>(PauseMenuClass);
		TestNotNull(TEXT("The pause-menu asset is a Widget Blueprint generated class"), GeneratedClass);
		if (GeneratedClass)
		{
			const UWidgetTree* WidgetTree = GeneratedClass->GetWidgetTreeArchetype();
			TestNotNull(TEXT("The pause-menu Widget Blueprint owns a widget tree"), WidgetTree);
			if (WidgetTree)
			{
				const UPanelWidget* ActionPanel =
					Cast<UPanelWidget>(WidgetTree->FindWidget(TEXT("PNL_ActionMenu")));
				const UPanelWidget* QuitConfirmPanel =
					Cast<UPanelWidget>(WidgetTree->FindWidget(TEXT("PNL_QuitConfirm")));
				const UTextBlock* HostPauseNotice =
					Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("TXT_HostPauseNotice")));

				TestNotNull(TEXT("The pause menu contains panel PNL_ActionMenu"), ActionPanel);
				TestNotNull(TEXT("The pause menu contains panel PNL_QuitConfirm"), QuitConfirmPanel);
				TestNotNull(TEXT("The pause menu contains text TXT_HostPauseNotice"), HostPauseNotice);
				if (HostPauseNotice)
				{
					TestEqual(
						TEXT("The replicated host-pause notice is Korean"),
						HostPauseNotice->GetText().ToString(),
						FString(TEXT("방장이 게임을 일시정지했습니다.")));
				}

				const UClass* PrimaryButtonClass = LoadClass<UWidget>(nullptr, LBRaidPausePrimaryButtonClassPath);
				TestNotNull(TEXT("The common Primary button class loads"), PrimaryButtonClass);
				TestRaidPauseContractButton(*this, WidgetTree, PrimaryButtonClass, TEXT("BTN_Resume"), ActionPanel, TEXT("계속하기"));
				TestRaidPauseContractButton(*this, WidgetTree, PrimaryButtonClass, TEXT("BTN_ReturnToRoom"), ActionPanel, TEXT("방으로 이동"));
				TestRaidPauseContractButton(*this, WidgetTree, PrimaryButtonClass, TEXT("BTN_Quit"), ActionPanel, TEXT("게임 종료"));
				TestRaidPauseContractButton(*this, WidgetTree, PrimaryButtonClass, TEXT("BTN_ConfirmQuit"), QuitConfirmPanel, TEXT("종료"));
				TestRaidPauseContractButton(*this, WidgetTree, PrimaryButtonClass, TEXT("BTN_CancelQuit"), QuitConfirmPanel, TEXT("취소"));
			}
		}
	}

	static const FName ControllerFunctionNames[] = {
		TEXT("TogglePauseMenu"),
		TEXT("ClosePauseMenu"),
		TEXT("CanRequestAbortRaidToRoom"),
		TEXT("RequestAbortRaidToRoom"),
		TEXT("ConfirmQuitGame")
	};
	for (const FName FunctionName : ControllerFunctionNames)
	{
		TestNotNull(
			*FString::Printf(TEXT("PlayerController exposes %s to the native widget"), *FunctionName.ToString()),
			ALB_PlayerController::StaticClass()->FindFunctionByName(FunctionName));
	}

	UWorld* InputWorld = UWorld::CreateWorld(
		EWorldType::Game,
		false,
		TEXT("LBRaidPauseMenuInputContractWorld"));
	TestNotNull(TEXT("A transient world can be created for the Escape binding contract"), InputWorld);
	if (InputWorld)
	{
		ALB_PlayerController* Controller = NewObject<ALB_PlayerController>(InputWorld->PersistentLevel);
		TestNotNull(TEXT("The Escape binding test controller is constructed"), Controller);
		if (Controller)
		{
			Controller->SetupInputComponent();
			TestNotNull(TEXT("SetupInputComponent creates an input component"), Controller->InputComponent.Get());
			if (Controller->InputComponent)
			{
				int32 EscapeBindingCount = 0;
				const FInputKeyBinding* EscapeBinding = nullptr;
				for (const FInputKeyBinding& KeyBinding : Controller->InputComponent->KeyBindings)
				{
					if (KeyBinding.Chord.Key == EKeys::Zero && KeyBinding.KeyEvent == IE_Pressed)
					{
						++EscapeBindingCount;
						EscapeBinding = &KeyBinding;
					}
				}

				TestEqual(TEXT("Escape has exactly one direct pressed binding"), EscapeBindingCount, 1);
				TestNotNull(TEXT("Escape is bound directly by the native controller"), EscapeBinding);
				if (EscapeBinding)
				{
					TestTrue(TEXT("Escape executes while the world is paused"), EscapeBinding->bExecuteWhenPaused);
					TestTrue(TEXT("Escape consumes the system-key press"), EscapeBinding->bConsumeInput);
					TestTrue(TEXT("Escape has a callable native delegate"), EscapeBinding->KeyDelegate.IsBound());
				}
			}

			Controller->bPauseMenuOpen = false;
			Controller->bPauseMenuOpenPending = true;
			Controller->TogglePauseMenu();
			TestFalse(
				TEXT("A second Escape cancels an async pending menu open"),
				Controller->bPauseMenuOpenPending);
		}

		InputWorld->DestroyWorld(false);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBRaidHostPauseReplicationTest,
	"LeftBehind.Raid.PauseMenu.HostPauseReplication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBRaidHostPauseReplicationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const UClass* GameStateClass = ALB_RaidGameState::StaticClass();
	const FBoolProperty* HostPauseProperty =
		FindFProperty<FBoolProperty>(GameStateClass, TEXT("bHostPauseActive"));
	TestNotNull(TEXT("RaidGameState exposes bHostPauseActive"), HostPauseProperty);
	if (HostPauseProperty)
	{
		TestTrue(
			TEXT("bHostPauseActive is a replicated property"),
			HostPauseProperty->HasAnyPropertyFlags(CPF_Net));
		TestTrue(
			TEXT("bHostPauseActive uses RepNotify"),
			HostPauseProperty->HasAnyPropertyFlags(CPF_RepNotify));

		TestEqual(
			TEXT("bHostPauseActive names the expected RepNotify handler"),
			HostPauseProperty->RepNotifyFunc,
			FName(TEXT("OnRep_HostPauseActive")));
	}

	TestNotNull(
		TEXT("RaidGameState exposes OnHostPauseChanged to UI"),
		FindFProperty<FMulticastDelegateProperty>(GameStateClass, TEXT("OnHostPauseChanged")));
	TestNotNull(
		TEXT("RaidGameState has the host-pause RepNotify handler"),
		GameStateClass->FindFunctionByName(TEXT("OnRep_HostPauseActive")));

	UWorld* TestWorld = UWorld::CreateWorld(
		EWorldType::Game,
		false,
		TEXT("LBRaidHostPauseReplicationWorld"));
	TestNotNull(TEXT("A transient authority world can be created for the host-pause setter"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	ALB_RaidGameState* RaidGameState = NewObject<ALB_RaidGameState>(TestWorld->PersistentLevel);
	TestNotNull(TEXT("The host-pause test GameState is constructed"), RaidGameState);
	if (RaidGameState)
	{
		TestTrue(TEXT("The host-pause setter test GameState has authority"), RaidGameState->HasAuthority());
		TestFalse(TEXT("Host pause starts inactive"), RaidGameState->bHostPauseActive);
		RaidGameState->SetHostPauseActive_ServerOnly(true);
		TestTrue(TEXT("The server-only setter activates host pause"), RaidGameState->bHostPauseActive);
		RaidGameState->SetHostPauseActive_ServerOnly(false);
		TestFalse(TEXT("The server-only setter clears host pause"), RaidGameState->bHostPauseActive);
	}

	TestWorld->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBRaidPausePolicyTest,
	"LeftBehind.Raid.PauseMenu.Policy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBRaidPausePolicyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* TestWorld = UWorld::CreateWorld(
		EWorldType::Game,
		false,
		TEXT("LBRaidPausePolicyWorld"));
	TestNotNull(TEXT("A transient standalone world can be created for pause policy checks"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	ALB_RaidGameMode* RaidGameMode = NewObject<ALB_RaidGameMode>(TestWorld->PersistentLevel);
	ALB_RaidGameState* RaidGameState = NewObject<ALB_RaidGameState>(TestWorld->PersistentLevel);
	APlayerController* HostController = NewObject<APlayerController>(TestWorld->PersistentLevel);
	APlayerController* RemoteController = NewObject<APlayerController>(TestWorld->PersistentLevel);
	TestNotNull(TEXT("The pause policy GameMode is constructed"), RaidGameMode);
	TestNotNull(TEXT("The pause policy GameState is constructed"), RaidGameState);
	TestNotNull(TEXT("The pause policy host controller is constructed"), HostController);
	TestNotNull(TEXT("The pause policy remote controller is constructed"), RemoteController);
	if (!RaidGameMode || !RaidGameState || !HostController || !RemoteController)
	{
		TestWorld->DestroyWorld(false);
		return false;
	}

	RaidGameMode->GameState = RaidGameState;
	HostController->SetAsLocalPlayerController();
	TestTrue(TEXT("The policy GameMode has authority"), RaidGameMode->HasAuthority());
	TestTrue(TEXT("The policy requester has authority"), HostController->HasAuthority());
	TestTrue(TEXT("The policy requester is local"), HostController->IsLocalController());
	TestEqual(TEXT("The policy world is standalone"), RaidGameMode->GetNetMode(), NM_Standalone);
	TestFalse(TEXT("A null requester can never pause the raid"), RaidGameMode->CanSetHostPause(nullptr));
	TestFalse(TEXT("A null requester can never abort the raid"), RaidGameMode->CanAbortRaidToRoom(nullptr));
	TestFalse(TEXT("A null requester cannot mutate host pause"), RaidGameMode->TrySetHostPause(nullptr, true));
	TestFalse(TEXT("A non-local controller cannot pause the raid"), RaidGameMode->CanSetHostPause(RemoteController));
	TestFalse(TEXT("A non-local controller cannot abort the raid"), RaidGameMode->CanAbortRaidToRoom(RemoteController));

	UWorld* OtherWorld = UWorld::CreateWorld(
		EWorldType::Game,
		false,
		TEXT("LBRaidPauseOtherWorld"));
	TestNotNull(TEXT("A second world can be created for requester isolation"), OtherWorld);
	if (OtherWorld)
	{
		APlayerController* OtherWorldController =
			NewObject<APlayerController>(OtherWorld->PersistentLevel);
		TestNotNull(TEXT("The other-world requester is constructed"), OtherWorldController);
		if (OtherWorldController)
		{
			OtherWorldController->SetAsLocalPlayerController();
			TestFalse(
				TEXT("A local authority controller from another world cannot pause this raid"),
				RaidGameMode->CanSetHostPause(OtherWorldController));
			TestFalse(
				TEXT("A local authority controller from another world cannot abort this raid"),
				RaidGameMode->CanAbortRaidToRoom(OtherWorldController));
		}
		OtherWorld->DestroyWorld(false);
	}

	RaidGameMode->bReturnTravelInProgress = true;
	TestFalse(TEXT("A pending travel blocks another pause acquisition"), RaidGameMode->CanSetHostPause(HostController));
	TestFalse(TEXT("A pending travel blocks another room return"), RaidGameMode->CanAbortRaidToRoom(HostController));
	RaidGameMode->bReturnTravelInProgress = false;

	struct FStatePolicyExpectation
	{
		ELBRaidState State;
		bool bActiveRaidActionAllowed;
		const TCHAR* StateName;
	};

	const FStatePolicyExpectation Expectations[] = {
		{ELBRaidState::Waiting, true, TEXT("Waiting")},
		{ELBRaidState::Countdown, true, TEXT("Countdown")},
		{ELBRaidState::Battle, true, TEXT("Battle")},
		{ELBRaidState::Result, false, TEXT("Result")}
	};

	for (const FStatePolicyExpectation& Expectation : Expectations)
	{
		RaidGameState->RaidState = Expectation.State;
		TestEqual(
			*FString::Printf(TEXT("Host pause policy matches the %s raid state"), Expectation.StateName),
			RaidGameMode->CanSetHostPause(HostController),
			Expectation.bActiveRaidActionAllowed);
		TestEqual(
			*FString::Printf(TEXT("Room-return abort policy matches the %s raid state"), Expectation.StateName),
			RaidGameMode->CanAbortRaidToRoom(HostController),
			Expectation.bActiveRaidActionAllowed);
		TestEqual(
			*FString::Printf(TEXT("The existing result return policy remains exclusive in %s"), Expectation.StateName),
			RaidGameMode->CanReturnToMainMenu(HostController),
			!Expectation.bActiveRaidActionAllowed);
	}

	RaidGameState->RaidState = ELBRaidState::Result;
	TestFalse(
		TEXT("TrySetHostPause cannot acquire a new world pause after Result begins"),
		RaidGameMode->TrySetHostPause(HostController, true));
	TestTrue(
		TEXT("The cleanup path is harmless when no host pause is owned"),
		RaidGameMode->TrySetHostPause(HostController, false));

	TestWorld->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
