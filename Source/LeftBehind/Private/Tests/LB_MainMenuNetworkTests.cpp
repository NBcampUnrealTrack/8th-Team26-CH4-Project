#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/EngineBaseTypes.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/EditableText.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

#include "GameMode/LB_MainMenuGameMode.h"
#include "GameState/LB_MainMenuGameState.h"
#include "Player/LB_MainMenuPlayerController.h"
#include "System/MainMenu/LB_LocalPlayerProfileSubsystem.h"
#include "System/Online/LB_OnlineInvitePolicy.h"
#include "System/Online/LB_OnlineLoginPolicy.h"
#include "System/Online/LB_OnlineSessionSubsystem.h"
#include "UI/MainMenu/LB_CodenameEntryWidget.h"
#include "UI/MainMenu/LB_MainMenuWaitingWidget.h"
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
	FLBMainMenuCodenameCacheTest,
	"LeftBehind.MainMenu.Codename.Cache",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBMainMenuCodenameCacheTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UGameInstance* FirstGameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UGameInstance* SecondGameInstance = NewObject<UGameInstance>(GetTransientPackage());
	ULB_LocalPlayerProfileSubsystem* FirstProfile =
		NewObject<ULB_LocalPlayerProfileSubsystem>(FirstGameInstance);
	ULB_LocalPlayerProfileSubsystem* SecondProfile =
		NewObject<ULB_LocalPlayerProfileSubsystem>(SecondGameInstance);
	TestNotNull(TEXT("A local profile cache can be created without online services"), FirstProfile);
	TestNotNull(TEXT("Each game instance receives an independent cache object"), SecondProfile);
	if (!FirstProfile || !SecondProfile)
	{
		return false;
	}

	TestFalse(TEXT("A new profile has no cached codename"), FirstProfile->HasCodename());
	const uint32 InitialRevision = FirstProfile->GetCodenameRevision();
	FString Sanitized;
	TestEqual(
		TEXT("Invalid input is rejected before it can enter the travel cache"),
		FirstProfile->TrySetCodename(TEXT("A"), Sanitized),
		ELBCodenameSubmitResult::TooShort);
	TestFalse(TEXT("Rejected input leaves the cache empty"), FirstProfile->HasCodename());
	TestEqual(TEXT("Rejected input does not advance the cache revision"), FirstProfile->GetCodenameRevision(), InitialRevision);

	TestEqual(
		TEXT("Accepted input is sanitized while it is stored"),
		FirstProfile->TrySetCodename(TEXT("  Raider  "), Sanitized),
		ELBCodenameSubmitResult::Accepted);
	TestEqual(TEXT("The sanitized value is returned"), Sanitized, FString(TEXT("Raider")));
	TestEqual(TEXT("Only the sanitized value is cached"), FirstProfile->GetCodename(), FString(TEXT("Raider")));
	TestTrue(TEXT("A successful update advances the cache revision"), FirstProfile->GetCodenameRevision() > InitialRevision);
	TestFalse(TEXT("Another local profile does not share the cached value"), SecondProfile->HasCodename());

	const uint32 StoredRevision = FirstProfile->GetCodenameRevision();
	FirstProfile->ClearCodename();
	TestFalse(TEXT("Clearing removes the cached codename"), FirstProfile->HasCodename());
	TestTrue(TEXT("Clearing invalidates in-flight submissions through the revision"), FirstProfile->GetCodenameRevision() > StoredRevision);
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
	/*TestEqual(
		TEXT("Default raid map is the production Main package"),
		GameModeCDO->GetRaidMap().ToSoftObjectPath().GetLongPackageName(),
		FString(TEXT("/Game/LeftBehind/Maps/Main")));*/
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
	TestNotNull(
		TEXT("The main menu exposes the room-name entry screen"),
		ALB_MainMenuPlayerController::StaticClass()->FindFunctionByName(TEXT("BeginRoomCreation")));
	TestNotNull(
		TEXT("The room-name entry screen can submit its draft"),
		ALB_MainMenuPlayerController::StaticClass()->FindFunctionByName(TEXT("SubmitRoomName")));
	TestFalse(
		TEXT("The native multiplayer hub can be instantiated without a Blueprint asset"),
		ULB_MultiplayerHubWidget::StaticClass()->HasAnyClassFlags(CLASS_Abstract));
	TestNotNull(
		TEXT("The legacy CreateRoom Blueprint entry point remains available"),
		ULB_OnlineSessionSubsystem::StaticClass()->FindFunctionByName(TEXT("CreateRoom")));
	TestNotNull(
		TEXT("The named-room Blueprint entry point is available"),
		ULB_OnlineSessionSubsystem::StaticClass()->FindFunctionByName(TEXT("CreateRoomWithName")));
	TestNotNull(
		TEXT("The active room name is available to the waiting room and Blueprints"),
		ULB_OnlineSessionSubsystem::StaticClass()->FindFunctionByName(TEXT("GetCurrentRoomName")));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBRoomNameValidationTest,
	"LeftBehind.MainMenu.Network.RoomNameValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBRoomNameValidationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FString NormalizedName;
	FText Error;
	TestTrue(
		TEXT("Korean, English, and numeric characters are accepted"),
		ULB_OnlineSessionSubsystem::ValidateRoomName(
			TEXT("한글Room26"), NormalizedName, Error));
	TestEqual(TEXT("Accepted names are preserved"), NormalizedName, FString(TEXT("한글Room26")));

	TestTrue(
		TEXT("Whitespace is removed before validation"),
		ULB_OnlineSessionSubsystem::ValidateRoomName(
			TEXT("  우리 방  26 "), NormalizedName, Error));
	TestEqual(TEXT("All whitespace is removed"), NormalizedName, FString(TEXT("우리방26")));

	TestFalse(
		TEXT("A line break is rejected instead of normalized away"),
		ULB_OnlineSessionSubsystem::ValidateRoomName(
			TEXT("Room\nName"), NormalizedName, Error));
	TestFalse(
		TEXT("Control characters are rejected"),
		ULB_OnlineSessionSubsystem::ValidateRoomName(
			FString(TEXT("Room")) + TCHAR(0x001F), NormalizedName, Error));
	TestFalse(
		TEXT("Punctuation is rejected"),
		ULB_OnlineSessionSubsystem::ValidateRoomName(
			TEXT("Room!"), NormalizedName, Error));
	TestFalse(
		TEXT("Names shorter than two normalized characters are rejected"),
		ULB_OnlineSessionSubsystem::ValidateRoomName(
			TEXT(" A "), NormalizedName, Error));
	TestFalse(
		TEXT("Names longer than 24 normalized characters are rejected"),
		ULB_OnlineSessionSubsystem::ValidateRoomName(
			FString::ChrN(25, TEXT('A')), NormalizedName, Error));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBRoomNameWidgetContractTest,
	"LeftBehind.MainMenu.Network.RoomNameWidgetContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBRoomNameWidgetContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const TCHAR* WidgetClassPath =
		TEXT("/Game/LeftBehind/UI/MainMenu/WBP_CodenameEntry.WBP_CodenameEntry_C");
	UClass* WidgetClass = LoadClass<ULB_CodenameEntryWidget>(nullptr, WidgetClassPath);
	TestNotNull(TEXT("The existing codename-entry designer asset loads"), WidgetClass);
	if (!WidgetClass)
	{
		return false;
	}

	TestTrue(
		TEXT("The designer asset directly uses the native reusable entry widget"),
		WidgetClass->GetSuperClass() == ULB_CodenameEntryWidget::StaticClass());
	const UWidgetBlueprintGeneratedClass* GeneratedClass =
		Cast<UWidgetBlueprintGeneratedClass>(WidgetClass);
	TestNotNull(TEXT("The entry asset is a Widget Blueprint generated class"), GeneratedClass);
	if (!GeneratedClass)
	{
		return false;
	}

	const UWidgetTree* WidgetTree = GeneratedClass->GetWidgetTreeArchetype();
	TestNotNull(TEXT("The entry asset owns a widget tree"), WidgetTree);
	if (!WidgetTree)
	{
		return false;
	}
	TestNotNull(
		TEXT("The entry root can host the native validation message"),
		Cast<UPanelWidget>(WidgetTree->RootWidget));

	TestNotNull(
		TEXT("Room-name mode reuses ETB_Name"),
		Cast<UEditableText>(WidgetTree->FindWidget(TEXT("ETB_Name"))));
	TestNotNull(
		TEXT("Room-name mode reuses TXT_CharCount"),
		Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("TXT_CharCount"))));
	for (const FName TextWidgetName : {
		FName(TEXT("TextBlock_31")),
		FName(TEXT("TextBlock_32")),
		FName(TEXT("TextBlock_33")),
		FName(TEXT("TextBlock_34")),
		FName(TEXT("TXT_Label_2")),
		FName(TEXT("TXT_Label_3")),
		FName(TEXT("TXT_Back"))})
	{
		TestNotNull(
			*FString::Printf(TEXT("Room-name mode reuses %s"), *TextWidgetName.ToString()),
			Cast<UTextBlock>(WidgetTree->FindWidget(TextWidgetName)));
	}

	UWidget* ConfirmButton = WidgetTree->FindWidget(TEXT("BTN_Confirm"));
	UWidget* BackButton = WidgetTree->FindWidget(TEXT("BTN_CodeName_Back"));
	TestNotNull(TEXT("Room-name mode reuses BTN_Confirm"), ConfirmButton);
	TestNotNull(TEXT("Room-name mode reuses BTN_CodeName_Back"), BackButton);

	const UClass* PrimaryButtonClass = LoadClass<UWidget>(
		nullptr,
		TEXT("/Game/LeftBehind/UI/SubWidgets/WBP_Common_ButtonPrimary.WBP_Common_ButtonPrimary_C"));
	TestNotNull(TEXT("The existing common primary button class loads"), PrimaryButtonClass);
	if (PrimaryButtonClass && ConfirmButton)
	{
		TestTrue(
			TEXT("The create-room action retains the common primary button styling"),
			ConfirmButton->GetClass()->IsChildOf(PrimaryButtonClass));
	}

	const auto HasSupportedClickDelegate = [](const UWidget* Widget)
	{
		return Widget
			&& (FindFProperty<FMulticastDelegateProperty>(Widget->GetClass(), TEXT("OnBTNClicked"))
				|| FindFProperty<FMulticastDelegateProperty>(Widget->GetClass(), TEXT("OnClicked")));
	};
	TestTrue(TEXT("BTN_Confirm exposes a supported click delegate"), HasSupportedClickDelegate(ConfirmButton));
	TestTrue(TEXT("BTN_CodeName_Back exposes a supported click delegate"), HasSupportedClickDelegate(BackButton));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBWaitingRoomWidgetContractTest,
	"LeftBehind.MainMenu.Network.WaitingRoomWidgetContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBWaitingRoomWidgetContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const TCHAR* WidgetClassPath =
		TEXT("/Game/LeftBehind/UI/MainMenu/WBP_WaitingRoom.WBP_WaitingRoom_C");
	UClass* WidgetClass = LoadClass<ULB_MainMenuWaitingWidget>(nullptr, WidgetClassPath);
	TestNotNull(TEXT("The waiting-room shell asset loads"), WidgetClass);
	if (!WidgetClass)
	{
		return false;
	}

	TestTrue(
		TEXT("The waiting-room shell directly uses the native designed widget"),
		WidgetClass->GetSuperClass() == ULB_MainMenuWaitingWidget::StaticClass());
	TestFalse(
		TEXT("The waiting-room shell remains constructible"),
		WidgetClass->HasAnyClassFlags(CLASS_Abstract));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
