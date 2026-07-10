#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "GameMode/LB_MainMenuGameMode.h"
#include "GameState/LB_MainMenuGameState.h"
#include "Player/LB_MainMenuPlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBMainMenuCodenameValidationTest,
	"LeftBehind.MainMenu.Codename.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBMainMenuCodenameValidationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FString Sanitized;

	TestEqual(
		TEXT("Two characters are accepted"),
		ALB_MainMenuGameMode::ValidateCodename(TEXT("AB"), Sanitized),
		ELBCodenameSubmitResult::Accepted);
	TestEqual(TEXT("Accepted value is unchanged"), Sanitized, FString(TEXT("AB")));

	TestEqual(
		TEXT("Twelve characters are accepted"),
		ALB_MainMenuGameMode::ValidateCodename(TEXT("ABCDEFGHIJKL"), Sanitized),
		ELBCodenameSubmitResult::Accepted);
	TestEqual(
		TEXT("Outer whitespace is trimmed"),
		ALB_MainMenuGameMode::ValidateCodename(TEXT("  Raider  "), Sanitized),
		ELBCodenameSubmitResult::Accepted);
	TestEqual(TEXT("Trimmed name is returned"), Sanitized, FString(TEXT("Raider")));

	TestEqual(
		TEXT("Whitespace-only input is rejected"),
		ALB_MainMenuGameMode::ValidateCodename(TEXT("   "), Sanitized),
		ELBCodenameSubmitResult::TooShort);
	TestEqual(
		TEXT("One character is rejected"),
		ALB_MainMenuGameMode::ValidateCodename(TEXT("A"), Sanitized),
		ELBCodenameSubmitResult::TooShort);
	TestEqual(
		TEXT("Thirteen characters are rejected rather than truncated"),
		ALB_MainMenuGameMode::ValidateCodename(TEXT("ABCDEFGHIJKLM"), Sanitized),
		ELBCodenameSubmitResult::TooLong);
	TestEqual(
		TEXT("Control characters are rejected"),
		ALB_MainMenuGameMode::ValidateCodename(TEXT("AB\nCD"), Sanitized),
		ELBCodenameSubmitResult::InvalidCharacters);
	TestEqual(
		TEXT("Unicode codenames follow the same length policy"),
		ALB_MainMenuGameMode::ValidateCodename(TEXT("헌터"), Sanitized),
		ELBCodenameSubmitResult::Accepted);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBMainMenuNativeDefaultsTest,
	"LeftBehind.MainMenu.Network.NativeDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBMainMenuNativeDefaultsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const ALB_MainMenuGameMode* GameModeCDO = GetDefault<ALB_MainMenuGameMode>();
	TestTrue(TEXT("Main menu uses seamless travel"), GameModeCDO->bUseSeamlessTravel);
	TestTrue(TEXT("Pawn-less menu skips RestartPlayer"), GameModeCDO->StartsPlayersWithoutMenuPawns());
	TestEqual(TEXT("Two players are required by default"), GameModeCDO->GetMinPlayersToStart(), 2);
	TestEqual(
		TEXT("Default raid map is the production Main package"),
		GameModeCDO->GetRaidMap().ToSoftObjectPath().GetLongPackageName(),
		FString(TEXT("/Game/LeftBehind/Maps/Main")));
	TestTrue(
		TEXT("Menu GameMode uses the replicated menu GameState"),
		GameModeCDO->GameStateClass->IsChildOf(ALB_MainMenuGameState::StaticClass()));
	TestNull(
		TEXT("Remote clients have no ServerStartHunt RPC surface"),
		ALB_MainMenuPlayerController::StaticClass()->FindFunctionByName(TEXT("ServerStartHunt")));
	TestTrue(
		TEXT("Menu PlayerController can deliver Client RPCs such as ClientTravelInternal"),
		GetDefault<ALB_MainMenuPlayerController>()->GetIsReplicated());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
