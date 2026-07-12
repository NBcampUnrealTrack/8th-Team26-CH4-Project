#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/EngineBaseTypes.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"

#include "GameMode/LB_MainMenuGameMode.h"
#include "GameState/LB_MainMenuGameState.h"
#include "Player/LB_MainMenuPlayerController.h"
#include "System/Online/LB_OnlineInvitePolicy.h"
#include "System/Online/LB_OnlineLoginPolicy.h"
#include "System/Online/LB_OnlineSessionSubsystem.h"
#include "UI/MainMenu/LB_MultiplayerHubWidget.h"

namespace
{
	class FLBInviteTestSessionInfo final : public FOnlineSessionInfo
	{
	public:
		explicit FLBInviteTestSessionInfo(const FString& InSessionId)
			: SessionId(FUniqueNetIdString::Create(InSessionId, TEXT("LBInviteTest")))
		{
		}

		virtual const uint8* GetBytes() const override { return SessionId->GetBytes(); }
		virtual int32 GetSize() const override { return SessionId->GetSize(); }
		virtual bool IsValid() const override { return SessionId->IsValid(); }
		virtual FString ToString() const override { return SessionId->ToString(); }
		virtual FString ToDebugString() const override { return SessionId->ToDebugString(); }
		virtual const FUniqueNetId& GetSessionId() const override { return *SessionId; }

	private:
		FUniqueNetIdStringRef SessionId;
	};

	FOnlineSessionSearchResult MakeInviteTestResult(
		const FString& SessionId,
		const FString& OwnerId = FString())
	{
		FOnlineSessionSearchResult Result;
		Result.Session.SessionInfo = MakeShared<FLBInviteTestSessionInfo>(SessionId);
		if (!OwnerId.IsEmpty())
		{
			Result.Session.OwningUserId = FUniqueNetIdString::Create(OwnerId, TEXT("LBInviteTest"));
			Result.Session.OwningUserName = OwnerId;
		}
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBOnlineLoginPolicyTest,
	"LeftBehind.MainMenu.Network.LoginPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBOnlineLoginPolicyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TestTrue(
		TEXT("Missing command-line credentials keep the Account Portal fallback"),
		LBOnlineLoginPolicy::SelectLoginRoute(TEXT("-game -log"))
			== LBOnlineLoginPolicy::ELoginRoute::AccountPortal);
	TestTrue(
		TEXT("Partial command-line credentials are rejected instead of opening a portal"),
		LBOnlineLoginPolicy::SelectLoginRoute(
			TEXT("-AUTH_LOGIN=localhost:8081 -AUTH_PASSWORD=Player1"))
			== LBOnlineLoginPolicy::ELoginRoute::Invalid);
	TestTrue(
		TEXT("An explicitly empty auth type is rejected instead of opening a portal"),
		LBOnlineLoginPolicy::SelectLoginRoute(TEXT("-AUTH_TYPE="))
			== LBOnlineLoginPolicy::ELoginRoute::Invalid);
	TestTrue(
		TEXT("Developer credentials select the engine AutoLogin path"),
		LBOnlineLoginPolicy::SelectLoginRoute(
			TEXT("-AUTH_TYPE=developer -AUTH_LOGIN=localhost:8081 -AUTH_PASSWORD=Player1"))
			== LBOnlineLoginPolicy::ELoginRoute::AutoLogin);
	TestTrue(
		TEXT("Any explicit auth type is delegated to the engine"),
		LBOnlineLoginPolicy::SelectLoginRoute(TEXT("-AUTH_TYPE=accountportal"))
			== LBOnlineLoginPolicy::ELoginRoute::AutoLogin);

	return true;
}

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
	FString DefaultPlatformService;
	TestTrue(
		TEXT("A default online service is configured"),
		GConfig->GetString(
			TEXT("OnlineSubsystem"),
			TEXT("DefaultPlatformService"),
			DefaultPlatformService,
			GEngineIni));
	TestEqual(TEXT("EOS is the default online service"), DefaultPlatformService, FString(TEXT("EOS")));

	FString PIEGameURLOptions;
	GConfig->GetString(
		TEXT("/Script/UnrealEd.EditorEngine"),
		TEXT("InEditorGameURLOptions"),
		PIEGameURLOptions,
		GEngineIni);
	const FURL PIEURL(nullptr, *FString::Printf(TEXT("/Game/LeftBehind/Maps/L_MainMenu%s"), *PIEGameURLOptions), TRAVEL_Absolute);
	TestFalse(TEXT("PIE keeps EOS P2P transport"), PIEURL.HasOption(TEXT("bUseIPSockets")));

	const ALB_MainMenuGameMode* GameModeCDO = GetDefault<ALB_MainMenuGameMode>();
	TestTrue(TEXT("Main menu uses seamless travel"), GameModeCDO->bUseSeamlessTravel);
	TestTrue(TEXT("Pawn-less menu skips RestartPlayer"), GameModeCDO->StartsPlayersWithoutMenuPawns());
	TestEqual(TEXT("One player may start a raid by default"), GameModeCDO->GetMinPlayersToStart(), 1);
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
	TestNotNull(
		TEXT("The main menu exposes the native EOS sign-in entry point"),
		ALB_MainMenuPlayerController::StaticClass()->FindFunctionByName(TEXT("BeginOnlinePlay")));
	TestFalse(
		TEXT("The native multiplayer hub can be instantiated without a Blueprint asset"),
		ULB_MultiplayerHubWidget::StaticClass()->HasAnyClassFlags(CLASS_Abstract));
	TestNotNull(
		TEXT("The online subsystem exposes social-overlay invitations"),
		ULB_OnlineSessionSubsystem::StaticClass()->FindFunctionByName(TEXT("OpenSocialOverlay")));
	TestNotNull(
		TEXT("The online subsystem exposes explicit room leave"),
		ULB_OnlineSessionSubsystem::StaticClass()->FindFunctionByName(TEXT("LeaveRoom")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBInviteOwnerResolutionFallbackTest,
	"LeftBehind.MainMenu.Network.InviteOwnerResolutionFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBInviteOwnerResolutionFallbackTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FOnlineSessionSearchResult OwnerlessInvite = MakeInviteTestResult(TEXT("Lobby-A"));
	TestFalse(
		TEXT("The UE generic validity check rejects an ownerless EOS invite"),
		OwnerlessInvite.IsValid());
	TestTrue(
		TEXT("The invite still contains joinable EOS session info"),
		OwnerlessInvite.IsSessionInfoValid());

	FOnlineSessionSearchResult PreparedInvite;
	TestTrue(
		TEXT("An ownerless invite with session info remains joinable"),
		LBOnlineInvitePolicy::PrepareAcceptedInvite(OwnerlessInvite, {}, PreparedInvite));
	TestTrue(
		TEXT("Preparing the invite preserves its session info"),
		PreparedInvite.IsSessionInfoValid());
	TestFalse(
		TEXT("No owner is invented when there is no matching cached result"),
		PreparedInvite.Session.OwningUserId.IsValid());

	const FOnlineSessionSearchResult MatchingCachedResult = MakeInviteTestResult(TEXT("Lobby-A"), TEXT("Host-A"));
	const FOnlineSessionSearchResult OtherCachedResult = MakeInviteTestResult(TEXT("Lobby-B"), TEXT("Host-B"));
	const TArray<FOnlineSessionSearchResult> CachedResults = {OtherCachedResult, MatchingCachedResult};
	TestTrue(
		TEXT("A matching cached room can enrich the accepted invite"),
		LBOnlineInvitePolicy::PrepareAcceptedInvite(OwnerlessInvite, CachedResults, PreparedInvite));
	TestTrue(
		TEXT("The matching cached owner is restored"),
		PreparedInvite.Session.OwningUserId.IsValid());
	TestEqual(
		TEXT("A different lobby owner is never copied"),
		PreparedInvite.Session.OwningUserName,
		FString(TEXT("Host-A")));
	TestEqual(
		TEXT("The invite keeps its original session id"),
		PreparedInvite.GetSessionIdStr(),
		FString(TEXT("Lobby-A")));

	FOnlineSessionSearchResult InvalidInvite;
	TestFalse(
		TEXT("An invite without session info is rejected"),
		LBOnlineInvitePolicy::PrepareAcceptedInvite(InvalidInvite, CachedResults, PreparedInvite));
	TestFalse(
		TEXT("A rejected invite clears any previously prepared payload"),
		PreparedInvite.IsSessionInfoValid());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
