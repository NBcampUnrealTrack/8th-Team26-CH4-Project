#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Online/OnlineSessionNames.h"
#include "System/Online/LB_OnlineSessionPolicy.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBOnlineTransportSelectionPolicyTest,
	"LeftBehind.Online.Policy.TransportSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBOnlineTransportSelectionPolicyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using LBOnlineSessionPolicy::ETransportMode;

	TestTrue(
		TEXT("EOS is always the production transport"),
		LBOnlineSessionPolicy::ResolveTransportMode(FName(TEXT("EOS")), false) == ETransportMode::EOS);
	TestTrue(
		TEXT("NULL becomes LAN only when the editor explicitly allows it"),
		LBOnlineSessionPolicy::ResolveTransportMode(FName(TEXT("NULL")), true) == ETransportMode::EditorLan);
	TestTrue(
		TEXT("A packaged build still rejects NULL fallback"),
		LBOnlineSessionPolicy::ResolveTransportMode(FName(TEXT("NULL")), false) == ETransportMode::Unsupported);
	TestTrue(
		TEXT("Unexpected providers remain unsupported"),
		LBOnlineSessionPolicy::ResolveTransportMode(FName(TEXT("Unexpected")), true) == ETransportMode::Unsupported);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBOnlineWaitingRoomPolicyTest,
	"LeftBehind.Online.Policy.WaitingRoom",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBOnlineWaitingRoomPolicyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FOnlineSessionSettings Settings = LBOnlineSessionPolicy::MakeWaitingRoomSettings();

	TestEqual(TEXT("A room has four public slots"), Settings.NumPublicConnections, 4);
	TestEqual(TEXT("A waiting room has no private slots"), Settings.NumPrivateConnections, 0);
	TestTrue(TEXT("A waiting room is advertised"), Settings.bShouldAdvertise);
	TestTrue(TEXT("A waiting room allows join in progress"), Settings.bAllowJoinInProgress);
	TestTrue(TEXT("A waiting room is lobby-backed"), Settings.bUseLobbiesIfAvailable);
	TestFalse(TEXT("Lobby voice chat is disabled"), Settings.bUseLobbiesVoiceChatIfAvailable);
	TestTrue(TEXT("Presence is enabled"), Settings.bUsesPresence);
	TestTrue(TEXT("Presence join is enabled while waiting"), Settings.bAllowJoinViaPresence);
	TestTrue(TEXT("Invites are enabled while waiting"), Settings.bAllowInvites);

	FString Phase;
	TestTrue(
		TEXT("Room phase is present"),
		Settings.Get(LBOnlineSessionPolicy::GetRoomPhaseKey(), Phase));
	TestEqual(TEXT("Room phase is Waiting"), Phase, FString(TEXT("Waiting")));

	bool bHostMigration = true;
	TestTrue(TEXT("Host migration setting is present"), Settings.Get(SETTING_HOST_MIGRATION, bHostMigration));
	TestFalse(TEXT("Host migration is disabled"), bHostMigration);
	const FOnlineSessionSetting* HostMigrationSetting = Settings.Settings.Find(SETTING_HOST_MIGRATION);
	TestNotNull(TEXT("Host migration has setting metadata"), HostMigrationSetting);
	if (HostMigrationSetting)
	{
		TestEqual(
			TEXT("Host migration is not advertised as a searchable attribute"),
			HostMigrationSetting->AdvertisementType,
			EOnlineDataAdvertisementType::DontAdvertise);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBOnlineLanFallbackPolicyTest,
	"LeftBehind.Online.Policy.EditorLanFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBOnlineLanFallbackPolicyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FOnlineSessionSettings Settings = LBOnlineSessionPolicy::MakeWaitingRoomSettings(true);

	TestTrue(TEXT("The editor fallback advertises a LAN match"), Settings.bIsLANMatch);
	TestTrue(TEXT("The LAN room remains discoverable"), Settings.bShouldAdvertise);
	TestFalse(TEXT("The LAN fallback does not request an EOS lobby"), Settings.bUseLobbiesIfAvailable);
	TestFalse(TEXT("The LAN fallback does not expose presence"), Settings.bUsesPresence);
	TestFalse(TEXT("The LAN fallback does not expose EOS invites"), Settings.bAllowInvites);

	LBOnlineSessionPolicy::ApplyInRaidPolicy(Settings, 2, true);
	TestFalse(TEXT("A raid is hidden from NULL LAN discovery"), Settings.bIsLANMatch);
	TestFalse(TEXT("The raid phase still rejects new LAN joins"), Settings.bAllowJoinInProgress);

	LBOnlineSessionPolicy::ApplyWaitingPolicy(Settings, true);
	TestTrue(TEXT("The LAN transport survives reopening the room"), Settings.bIsLANMatch);
	TestTrue(TEXT("The reopened LAN room is joinable again"), Settings.bAllowJoinInProgress);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBOnlineInRaidPolicyTest,
	"LeftBehind.Online.Policy.InRaid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBOnlineInRaidPolicyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FOnlineSessionSettings Settings = LBOnlineSessionPolicy::MakeWaitingRoomSettings();
	LBOnlineSessionPolicy::ApplyInRaidPolicy(Settings, 3);

	TestEqual(TEXT("An active raid has no public slots"), Settings.NumPublicConnections, 0);
	TestEqual(TEXT("Existing members occupy private capacity"), Settings.NumPrivateConnections, 3);
	TestFalse(TEXT("An active raid is not advertised"), Settings.bShouldAdvertise);
	TestFalse(TEXT("An active raid rejects join in progress"), Settings.bAllowJoinInProgress);
	TestFalse(TEXT("An active raid rejects invites"), Settings.bAllowInvites);
	TestFalse(TEXT("An active raid rejects presence join"), Settings.bAllowJoinViaPresence);
	TestTrue(TEXT("The existing lobby remains presence-backed"), Settings.bUsesPresence);

	FString Phase;
	TestTrue(
		TEXT("Raid phase is present"),
		Settings.Get(LBOnlineSessionPolicy::GetRoomPhaseKey(), Phase));
	TestEqual(TEXT("Room phase is InRaid"), Phase, FString(TEXT("InRaid")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBOnlineExclusiveOperationGateTest,
	"LeftBehind.Online.Policy.ExclusiveOperationGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBOnlineExclusiveOperationGateTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TestTrue(
		TEXT("An idle subsystem accepts an operation"),
		LBOnlineSessionPolicy::CanStartExclusiveOperation(false, false, false));
	TestFalse(
		TEXT("A pending online operation blocks a duplicate operation"),
		LBOnlineSessionPolicy::CanStartExclusiveOperation(true, false, false));
	TestFalse(
		TEXT("Connection travel blocks a new operation"),
		LBOnlineSessionPolicy::CanStartExclusiveOperation(false, true, false));
	TestFalse(
		TEXT("Menu travel blocks a new operation"),
		LBOnlineSessionPolicy::CanStartExclusiveOperation(false, false, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBOnlineInvitePolicyTest,
	"LeftBehind.Online.Policy.InviteAcceptance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBOnlineInvitePolicyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	constexpr int32 ExpectedBuild = 42;
	FOnlineSessionSettings WaitingSettings = LBOnlineSessionPolicy::MakeWaitingRoomSettings();
	WaitingSettings.BuildUniqueId = ExpectedBuild;
	TestTrue(
		TEXT("A same-build waiting room with a public slot accepts an invite"),
		LBOnlineSessionPolicy::CanAcceptInvite(WaitingSettings, 1, ExpectedBuild));

	FOnlineSessionSettings InRaidSettings = WaitingSettings;
	LBOnlineSessionPolicy::ApplyInRaidPolicy(InRaidSettings, 2);
	TestFalse(
		TEXT("An invite cannot enter a raid even if an old overlay invite remains"),
		LBOnlineSessionPolicy::CanAcceptInvite(InRaidSettings, 1, ExpectedBuild));
	TestFalse(
		TEXT("A full waiting room rejects an invite"),
		LBOnlineSessionPolicy::CanAcceptInvite(WaitingSettings, 0, ExpectedBuild));
	TestFalse(
		TEXT("A different game build rejects an invite"),
		LBOnlineSessionPolicy::CanAcceptInvite(WaitingSettings, 1, ExpectedBuild + 1));

	FOnlineSessionSettings MissingPhaseSettings = WaitingSettings;
	MissingPhaseSettings.Settings.Remove(LBOnlineSessionPolicy::GetRoomPhaseKey());
	TestFalse(
		TEXT("An invite without an explicit room phase is rejected"),
		LBOnlineSessionPolicy::CanAcceptInvite(MissingPhaseSettings, 1, ExpectedBuild));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBOnlineCurrentRoomSelectionTest,
	"LeftBehind.Online.Policy.CurrentRoomSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBOnlineCurrentRoomSelectionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FLBRoomSummary CurrentRoom;
	CurrentRoom.RoomId = TEXT("current-room");
	CurrentRoom.bCanJoin = true;
	const TArray<FLBRoomSummary> Rooms = {CurrentRoom};

	TestTrue(
		TEXT("The current joinable room id is accepted"),
		LBOnlineSessionPolicy::IsCurrentJoinSelection(Rooms, TEXT("current-room")));
	TestFalse(
		TEXT("A room id retained from an older search is rejected"),
		LBOnlineSessionPolicy::IsCurrentJoinSelection(Rooms, TEXT("stale-room")));
	TestFalse(
		TEXT("An empty selection is rejected"),
		LBOnlineSessionPolicy::IsCurrentJoinSelection(Rooms, FString()));

	FLBRoomSummary FullRoom = CurrentRoom;
	FullRoom.bCanJoin = false;
	TestFalse(
		TEXT("A full room from the current search is rejected"),
		LBOnlineSessionPolicy::IsCurrentJoinSelection({FullRoom}, TEXT("current-room")));
	TestFalse(
		TEXT("Duplicate ids are treated as an invalid snapshot"),
		LBOnlineSessionPolicy::IsCurrentJoinSelection({CurrentRoom, CurrentRoom}, TEXT("current-room")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
