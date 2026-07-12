#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameState/LB_RaidGameState.h"
#include "Player/LB_PlayerState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBPartyRosterChangeNotificationTest,
	"LeftBehind.Raid.Party.RosterChangeNotification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBPartyRosterChangeNotificationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* TestWorld = UWorld::CreateWorld(
		EWorldType::Game,
		false,
		TEXT("LBPartyRosterChangeNotificationWorld"));
	TestNotNull(TEXT("A transient world can be created for the party roster contract"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	ALB_RaidGameState* RaidGameState = NewObject<ALB_RaidGameState>(TestWorld->PersistentLevel);
	ALB_PlayerState* PlayerState = NewObject<ALB_PlayerState>(TestWorld->PersistentLevel);
	ALB_PlayerState* UntrackedPlayerState = NewObject<ALB_PlayerState>(TestWorld->PersistentLevel);
	TestNotNull(TEXT("The test raid game state is constructed"), RaidGameState);
	TestNotNull(TEXT("The tracked player state is constructed"), PlayerState);
	TestNotNull(TEXT("The untracked player state is constructed"), UntrackedPlayerState);

	if (!RaidGameState || !PlayerState || !UntrackedPlayerState)
	{
		TestWorld->DestroyWorld(false);
		return false;
	}

	int32 NotificationCount = 0;
	TArray<int32> ObservedRosterSizes;
	const FDelegateHandle NotificationHandle = RaidGameState->OnRaidPlayerArrayChanged.AddLambda(
		[&NotificationCount, &ObservedRosterSizes, RaidGameState]()
		{
			++NotificationCount;
			ObservedRosterSizes.Add(RaidGameState->PlayerArray.Num());
		});

	RaidGameState->AddPlayerState(PlayerState);
	TestTrue(TEXT("Adding a player inserts it into PlayerArray"), RaidGameState->PlayerArray.Contains(PlayerState));
	TestEqual(TEXT("Adding a player broadcasts one roster notification"), NotificationCount, 1);
	TestEqual(
		TEXT("The add notification observes the already-updated roster"),
		ObservedRosterSizes.IsValidIndex(0) ? ObservedRosterSizes[0] : INDEX_NONE,
		1);

	RaidGameState->AddPlayerState(PlayerState);
	TestEqual(TEXT("Adding the same player again does not duplicate it"), RaidGameState->PlayerArray.Num(), 1);
	TestEqual(TEXT("A duplicate add does not broadcast a false roster change"), NotificationCount, 1);

	RaidGameState->RemovePlayerState(UntrackedPlayerState);
	TestEqual(TEXT("Removing an untracked player does not broadcast a false roster change"), NotificationCount, 1);

	RaidGameState->RemovePlayerState(PlayerState);
	TestFalse(TEXT("Removing a player erases it from PlayerArray"), RaidGameState->PlayerArray.Contains(PlayerState));
	TestEqual(TEXT("Removing a player broadcasts the second roster notification"), NotificationCount, 2);
	TestEqual(
		TEXT("The remove notification observes the already-updated roster"),
		ObservedRosterSizes.IsValidIndex(1) ? ObservedRosterSizes[1] : INDEX_NONE,
		0);

	RaidGameState->OnRaidPlayerArrayChanged.Remove(NotificationHandle);
	TestWorld->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBCurrentLevelRosterFilteringTest,
	"LeftBehind.Raid.Party.CurrentLevelRosterFiltering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBCurrentLevelRosterFilteringTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UWorld* TestWorld = UWorld::CreateWorld(
		EWorldType::Game,
		false,
		TEXT("LBCurrentLevelRosterFilteringWorld"));
	TestNotNull(TEXT("A transient world can be created for the current-level roster contract"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	ALB_RaidGameState* RaidGameState = NewObject<ALB_RaidGameState>(TestWorld->PersistentLevel);
	ALB_PlayerState* CurrentPlayerState = NewObject<ALB_PlayerState>(TestWorld->PersistentLevel);
	ALB_PlayerState* PreviousLevelPlayerState = NewObject<ALB_PlayerState>(TestWorld->PersistentLevel);
	TestNotNull(TEXT("The filtering test raid game state is constructed"), RaidGameState);
	TestNotNull(TEXT("The current-level player state is constructed"), CurrentPlayerState);
	TestNotNull(TEXT("The previous-level player state is constructed"), PreviousLevelPlayerState);

	if (!RaidGameState || !CurrentPlayerState || !PreviousLevelPlayerState)
	{
		TestWorld->DestroyWorld(false);
		return false;
	}

	CurrentPlayerState->SetIsFromPreviousLevel(false);
	PreviousLevelPlayerState->SetIsFromPreviousLevel(true);
	RaidGameState->AddPlayerState(CurrentPlayerState);
	RaidGameState->AddPlayerState(PreviousLevelPlayerState);

	TestTrue(
		TEXT("The raw replicated roster contains the current-level player state"),
		RaidGameState->PlayerArray.Contains(CurrentPlayerState));
	TestTrue(
		TEXT("The raw replicated roster can temporarily retain a seamless-travel player state"),
		RaidGameState->PlayerArray.Contains(PreviousLevelPlayerState));

	TArray<ALB_PlayerState*> CurrentRaidPlayerStates;
	RaidGameState->GetCurrentRaidPlayerStates(CurrentRaidPlayerStates);

	TestEqual(TEXT("The filtered party roster contains exactly one current-level player"), CurrentRaidPlayerStates.Num(), 1);
	TestTrue(TEXT("The filtered party roster contains the current-level player"), CurrentRaidPlayerStates.Contains(CurrentPlayerState));
	TestFalse(TEXT("The filtered party roster excludes the previous-level player"), CurrentRaidPlayerStates.Contains(PreviousLevelPlayerState));

	TestWorld->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
