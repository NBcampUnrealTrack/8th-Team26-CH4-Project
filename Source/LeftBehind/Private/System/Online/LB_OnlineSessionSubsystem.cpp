#include "System/Online/LB_OnlineSessionSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogLBOnlineSession, Log, All);

namespace
{
	constexpr int32 LBLocalUserNum = 0;
	const FName LBMainMenuMap(TEXT("/Game/LeftBehind/Maps/L_MainMenu"));

	namespace LBOnlineSessionPolicy
	{
		constexpr int32 MaxPublicConnections = 4;
		constexpr int32 MaxSearchResults = 50;
		const FName RoomPhaseKey(TEXT("ROOM_PHASE"));
		const FString WaitingPhaseValue(TEXT("Waiting"));
		const FString InRaidPhaseValue(TEXT("InRaid"));

		FName GetRoomPhaseKey()
		{
			return RoomPhaseKey;
		}

		const FString& GetWaitingPhaseValue()
		{
			return WaitingPhaseValue;
		}

		const FString& GetInRaidPhaseValue()
		{
			return InRaidPhaseValue;
		}

		void ApplyWaitingPolicy(FOnlineSessionSettings& Settings)
		{
			Settings.NumPublicConnections = MaxPublicConnections;
			Settings.NumPrivateConnections = 0;
			Settings.bShouldAdvertise = true;
			Settings.bAllowJoinInProgress = true;
			Settings.bIsLANMatch = false;
			Settings.bIsDedicated = false;
			Settings.bUsesStats = false;
			Settings.bAllowInvites = true;
			Settings.bUsesPresence = true;
			Settings.bAllowJoinViaPresence = true;
			Settings.bAllowJoinViaPresenceFriendsOnly = false;
			Settings.bAntiCheatProtected = false;
			Settings.bUseLobbiesIfAvailable = true;
			Settings.bUseLobbiesVoiceChatIfAvailable = false;
			Settings.Set(
				GetRoomPhaseKey(),
				GetWaitingPhaseValue(),
				EOnlineDataAdvertisementType::ViaOnlineService);
			Settings.Set(
				SETTING_HOST_MIGRATION,
				false,
				EOnlineDataAdvertisementType::DontAdvertise);
		}

		FOnlineSessionSettings MakeWaitingRoomSettings()
		{
			FOnlineSessionSettings Settings;
			ApplyWaitingPolicy(Settings);
			return Settings;
		}

		void ApplyInRaidPolicy(FOnlineSessionSettings& Settings, const int32 CurrentPlayers)
		{
			Settings.NumPublicConnections = 0;
			Settings.NumPrivateConnections = FMath::Clamp(CurrentPlayers, 1, MaxPublicConnections);
			Settings.bShouldAdvertise = false;
			Settings.bAllowJoinInProgress = false;
			Settings.bAllowInvites = false;
			Settings.bUsesPresence = true;
			Settings.bAllowJoinViaPresence = false;
			Settings.bAllowJoinViaPresenceFriendsOnly = false;
			Settings.bIsLANMatch = false;
			Settings.bUseLobbiesIfAvailable = true;
			Settings.bUseLobbiesVoiceChatIfAvailable = false;
			Settings.Set(
				GetRoomPhaseKey(),
				GetInRaidPhaseValue(),
				EOnlineDataAdvertisementType::ViaOnlineService);
			Settings.Set(
				SETTING_HOST_MIGRATION,
				false,
				EOnlineDataAdvertisementType::DontAdvertise);
		}

		bool CanStartExclusiveOperation(
			const bool bHasPendingOperation,
			const bool bConnectionTravelPending,
			const bool bMenuTravelPending)
		{
			return !bHasPendingOperation && !bConnectionTravelPending && !bMenuTravelPending;
		}

		bool CanAcceptInvite(
			const FOnlineSessionSettings& InviteSettings,
			const int32 NumOpenPublicConnections,
			const int32 ExpectedBuildUniqueId)
		{
			FString Phase;
			return InviteSettings.BuildUniqueId == ExpectedBuildUniqueId
				&& InviteSettings.Get(GetRoomPhaseKey(), Phase)
				&& Phase == GetWaitingPhaseValue()
				&& InviteSettings.NumPublicConnections > 0
				&& NumOpenPublicConnections > 0;
		}

		bool IsCurrentJoinSelection(const TArray<FLBRoomSummary>& Rooms, const FString& RoomId)
		{
			if (RoomId.IsEmpty())
			{
				return false;
			}

			int32 MatchingRooms = 0;
			bool bMatchingRoomCanJoin = false;
			for (const FLBRoomSummary& Room : Rooms)
			{
				if (Room.RoomId == RoomId)
				{
					++MatchingRooms;
					bMatchingRoomCanJoin = Room.bCanJoin;
				}
			}

			return MatchingRooms == 1 && bMatchingRoomCanJoin;
		}
	}

	enum class ELBPendingOnlineOperation : uint8
	{
		None,
		Login,
		Create,
		Search,
		Join,
		Leave,
		UpdatePhase
	};

	FText MakeOnlineError(const FText& Action, const FString& Detail)
	{
		if (Detail.IsEmpty())
		{
			return FText::Format(
				NSLOCTEXT("LeftBehind", "OnlineActionFailed", "{0}에 실패했습니다. 잠시 후 다시 시도해 주세요."),
				Action);
		}

		return FText::Format(
			NSLOCTEXT("LeftBehind", "OnlineActionFailedWithDetail", "{0}에 실패했습니다: {1}"),
			Action,
			FText::FromString(Detail));
	}

	FText MakeEOSUnavailableError()
	{
		return NSLOCTEXT(
			"LeftBehind",
			"EOSUnavailable",
			"EOS를 초기화하지 못했습니다. DefaultEngine.ini의 EOS 기본값과 Config/GeneratedEngine.ini의 LeftBehindDev Artifact를 확인한 뒤 Unreal Editor를 완전히 다시 시작해 주세요.");
	}
}

class FLBOnlineSessionRuntime : public TSharedFromThis<FLBOnlineSessionRuntime>
{
public:
	explicit FLBOnlineSessionRuntime(ULB_OnlineSessionSubsystem* InOwner)
		: Owner(InOwner)
	{
	}

	void Initialize()
	{
		ULB_OnlineSessionSubsystem* OwnerSubsystem = Owner.Get();
		if (!IsValid(OwnerSubsystem))
		{
			return;
		}

		PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddSP(
			AsShared(),
			&FLBOnlineSessionRuntime::HandlePostLoadMap);
		if (GEngine)
		{
			NetworkFailureHandle = GEngine->OnNetworkFailure().AddSP(
				AsShared(),
				&FLBOnlineSessionRuntime::HandleNetworkFailure);
			TravelFailureHandle = GEngine->OnTravelFailure().AddSP(
				AsShared(),
				&FLBOnlineSessionRuntime::HandleTravelFailure);
		}

		OnlineSubsystem = Online::GetSubsystem(
			OwnerSubsystem->GetWorld(),
			FName(TEXT("EOS")));
		if (!OnlineSubsystem)
		{
			ReportError(MakeEOSUnavailableError(), ELBOnlineState::Error);
			return;
		}

		if (OnlineSubsystem->GetSubsystemName() != FName(TEXT("EOS")))
		{
			ReportError(
				FText::Format(
					NSLOCTEXT("LeftBehind", "WrongOnlineSubsystem", "EOS 대신 {0} 온라인 서비스가 선택되었습니다. DefaultEngine.ini를 확인해 주세요."),
					FText::FromName(OnlineSubsystem->GetSubsystemName())),
				ELBOnlineState::Error);
			return;
		}

		Identity = OnlineSubsystem->GetIdentityInterface();
		Sessions = OnlineSubsystem->GetSessionInterface();
		ExternalUI = OnlineSubsystem->GetExternalUIInterface();
		if (!Identity.IsValid() || !Sessions.IsValid())
		{
			ReportError(
				NSLOCTEXT("LeftBehind", "OnlineInterfacesMissing", "EOS 로그인 또는 방 기능을 불러오지 못했습니다. 프로젝트 플러그인과 Artifact 설정을 확인해 주세요."),
				ELBOnlineState::Error);
			return;
		}

		InviteAcceptedHandle = Sessions->AddOnSessionUserInviteAcceptedDelegate_Handle(
			FOnSessionUserInviteAcceptedDelegate::CreateSP(
				AsShared(),
				&FLBOnlineSessionRuntime::HandleInviteAccepted));
		SessionFailureHandle = Sessions->AddOnSessionFailureDelegate_Handle(
			FOnSessionFailureDelegate::CreateSP(
				AsShared(),
				&FLBOnlineSessionRuntime::HandleSessionFailure));

		if (FNamedOnlineSession* NamedSession = Sessions->GetNamedSession(NAME_GameSession))
		{
			bInRoom = true;
			bRoomHost = NamedSession->bHosting;
			FString Phase;
			if (NamedSession->SessionSettings.Get(LBOnlineSessionPolicy::GetRoomPhaseKey(), Phase)
				&& Phase == LBOnlineSessionPolicy::GetInRaidPhaseValue())
			{
				CurrentRoomPhase = ELBRoomPhase::InRaid;
			}
		}

		ClearLastError();
		if (bInRoom)
		{
			OwnerSubsystem->SetState(ELBOnlineState::InRoom);
		}
		else if (IsLoggedIn())
		{
			OwnerSubsystem->SetState(ELBOnlineState::Ready);
		}
		else
		{
			OwnerSubsystem->SetState(ELBOnlineState::SignedOut);
		}
	}

	void Shutdown()
	{
		ClearOperationDelegates();

		if (Sessions.IsValid())
		{
			if (InviteAcceptedHandle.IsValid())
			{
				Sessions->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteAcceptedHandle);
			}
			if (SessionFailureHandle.IsValid())
			{
				Sessions->ClearOnSessionFailureDelegate_Handle(SessionFailureHandle);
			}
		}

		InviteAcceptedHandle.Reset();
		SessionFailureHandle.Reset();
		if (PostLoadMapHandle.IsValid())
		{
			FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
			PostLoadMapHandle.Reset();
		}
		if (GEngine)
		{
			GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
			GEngine->OnTravelFailure().Remove(TravelFailureHandle);
		}
		NetworkFailureHandle.Reset();
		TravelFailureHandle.Reset();

		ActiveSearch.Reset();
		RoomResultIndices.Reset();
		ExternalUI.Reset();
		Sessions.Reset();
		Identity.Reset();
		OnlineSubsystem = nullptr;
		Owner.Reset();
	}

	bool SignIn()
	{
		if (!Identity.IsValid())
		{
			ReportError(
				NSLOCTEXT("LeftBehind", "SignInUnavailable", "EOS 로그인 기능을 사용할 수 없습니다."),
				ELBOnlineState::Error);
			return false;
		}

		if (IsLoggedIn())
		{
			ClearLastError();
			if (ULB_OnlineSessionSubsystem* OwnerSubsystem = Owner.Get())
			{
				OwnerSubsystem->SetState(bInRoom ? ELBOnlineState::InRoom : ELBOnlineState::Ready);
			}
			return true;
		}

		if (!BeginOperation(ELBPendingOnlineOperation::Login, ELBOnlineState::SigningIn))
		{
			return false;
		}

		LoginCompleteHandle = Identity->AddOnLoginCompleteDelegate_Handle(
			LBLocalUserNum,
			FOnLoginCompleteDelegate::CreateSP(
				AsShared(),
				&FLBOnlineSessionRuntime::HandleLoginComplete));
		// Online PIE Play Credentials complete before the game instance is created.
		// If no stored PIE login exists, use Account Portal without launch arguments.
		FOnlineAccountCredentials AccountPortalCredentials;
		AccountPortalCredentials.Type = TEXT("accountportal");
		const bool bLoginStarted = Identity->Login(LBLocalUserNum, AccountPortalCredentials);

		if (!bLoginStarted)
		{
			ClearLoginDelegate();
			PendingOperation = ELBPendingOnlineOperation::None;
			ReportError(
				NSLOCTEXT("LeftBehind", "AccountPortalLoginNotStarted", "EOS Account Portal 로그인을 시작하지 못했습니다. Artifact와 EAS 설정을 확인해 주세요."),
				ELBOnlineState::Error);
			return false;
		}

		return true;
	}

	bool CreateRoom()
	{
		if (!CanUseReadyServices(NSLOCTEXT("LeftBehind", "CreateRoomAction", "방 만들기")))
		{
			return false;
		}

		if (bInRoom || Sessions->GetNamedSession(NAME_GameSession))
		{
			ReportNonFatalError(NSLOCTEXT("LeftBehind", "AlreadyInRoom", "이미 방에 들어와 있습니다. 먼저 현재 방에서 나가 주세요."));
			return false;
		}

		if (!BeginOperation(ELBPendingOnlineOperation::Create, ELBOnlineState::Creating))
		{
			return false;
		}

		CreateCompleteHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
			FOnCreateSessionCompleteDelegate::CreateSP(
				AsShared(),
				&FLBOnlineSessionRuntime::HandleCreateComplete));
		const FOnlineSessionSettings Settings = LBOnlineSessionPolicy::MakeWaitingRoomSettings();
		if (!Sessions->CreateSession(LBLocalUserNum, NAME_GameSession, Settings))
		{
			ClearCreateDelegate();
			PendingOperation = ELBPendingOnlineOperation::None;
			ReportError(MakeOnlineError(NSLOCTEXT("LeftBehind", "CreateRoomAction", "방 만들기"), FString()), ELBOnlineState::Ready);
			return false;
		}

		return true;
	}

	bool RefreshRooms()
	{
		if (!CanUseReadyServices(NSLOCTEXT("LeftBehind", "RefreshRoomsAction", "방 목록 새로고침")))
		{
			return false;
		}

		if (bInRoom)
		{
			ReportNonFatalError(NSLOCTEXT("LeftBehind", "SearchWhileInRoom", "방에 들어간 상태에서는 다른 방을 검색할 수 없습니다."));
			return false;
		}

		if (!BeginOperation(ELBPendingOnlineOperation::Search, ELBOnlineState::Searching))
		{
			return false;
		}

		ActiveSearch = MakeShared<FOnlineSessionSearch>();
		ActiveSearch->MaxSearchResults = LBOnlineSessionPolicy::MaxSearchResults;
		ActiveSearch->bIsLanQuery = false;
		ActiveSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
		ActiveSearch->QuerySettings.Set(
			FName(TEXT("BuildUniqueId")),
			GetBuildUniqueId(),
			EOnlineComparisonOp::Equals);
		ActiveSearch->QuerySettings.Set(
			LBOnlineSessionPolicy::GetRoomPhaseKey(),
			LBOnlineSessionPolicy::GetWaitingPhaseValue(),
			EOnlineComparisonOp::Equals);

		RoomResultIndices.Reset();
		if (ULB_OnlineSessionSubsystem* OwnerSubsystem = Owner.Get())
		{
			OwnerSubsystem->SetRooms({});
		}

		FindCompleteHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
			FOnFindSessionsCompleteDelegate::CreateSP(
				AsShared(),
				&FLBOnlineSessionRuntime::HandleFindComplete));
		if (!Sessions->FindSessions(LBLocalUserNum, ActiveSearch.ToSharedRef()))
		{
			ClearFindDelegate();
			PendingOperation = ELBPendingOnlineOperation::None;
			ActiveSearch.Reset();
			ReportError(MakeOnlineError(NSLOCTEXT("LeftBehind", "RefreshRoomsAction", "방 목록 새로고침"), FString()), ELBOnlineState::Ready);
			return false;
		}

		return true;
	}

	bool JoinRoom(const FString& RoomId)
	{
		ULB_OnlineSessionSubsystem* OwnerSubsystem = Owner.Get();
		if (!CanUseReadyServices(NSLOCTEXT("LeftBehind", "JoinRoomAction", "방 입장")) || !OwnerSubsystem)
		{
			return false;
		}

		if (bInRoom || Sessions->GetNamedSession(NAME_GameSession))
		{
			ReportNonFatalError(NSLOCTEXT("LeftBehind", "JoinAlreadyInRoom", "이미 다른 방에 들어와 있습니다. 현재 방에서 나간 뒤 다시 시도해 주세요."));
			return false;
		}

		if (!LBOnlineSessionPolicy::IsCurrentJoinSelection(OwnerSubsystem->Rooms, RoomId))
		{
			ReportNonFatalError(NSLOCTEXT("LeftBehind", "StaleRoomSelection", "선택한 방 정보가 오래되었거나 방이 가득 찼습니다. 새로고침 후 다시 선택해 주세요."));
			return false;
		}

		const int32* ResultIndex = RoomResultIndices.Find(RoomId);
		if (!ResultIndex || !ActiveSearch.IsValid() || !ActiveSearch->SearchResults.IsValidIndex(*ResultIndex))
		{
			ReportNonFatalError(NSLOCTEXT("LeftBehind", "MissingRoomResult", "선택한 방이 최신 검색 결과에 없습니다. 새로고침 후 다시 선택해 주세요."));
			return false;
		}

		FOnlineSessionSearchResult DesiredSession = ActiveSearch->SearchResults[*ResultIndex];
		if (DesiredSession.GetSessionIdStr() != RoomId)
		{
			ReportNonFatalError(NSLOCTEXT("LeftBehind", "ChangedRoomResult", "방 목록이 변경되었습니다. 새로고침 후 다시 선택해 주세요."));
			return false;
		}

		return BeginJoin(MoveTemp(DesiredSession));
	}

	bool LeaveRoom()
	{
		if (!Sessions.IsValid())
		{
			ReportNonFatalError(NSLOCTEXT("LeftBehind", "LeaveUnavailable", "EOS 방 기능을 사용할 수 없습니다."));
			return false;
		}

		if (!bInRoom && !Sessions->GetNamedSession(NAME_GameSession))
		{
			ReportNonFatalError(NSLOCTEXT("LeftBehind", "NotInRoom", "현재 들어가 있는 방이 없습니다."));
			return false;
		}

		if (!BeginOperation(ELBPendingOnlineOperation::Leave, ELBOnlineState::Leaving))
		{
			return false;
		}

		return BeginDestroySession(true, FText::GetEmpty());
	}

	bool OpenSocialOverlay()
	{
		FText UnavailableReason;
		if (!CanOpenSocialOverlay(&UnavailableReason))
		{
			ReportNonFatalError(UnavailableReason);
			return false;
		}

		ClearLastError();
		if (!ExternalUI->ShowFriendsUI(LBLocalUserNum))
		{
			ReportNonFatalError(NSLOCTEXT("LeftBehind", "OverlayOpenFailed", "EOS 친구 창을 열지 못했습니다. 오버레이 설치와 친구 권한을 확인해 주세요."));
			return false;
		}

		return true;
	}

	bool CanOpenSocialOverlay(FText* OutUnavailableReason = nullptr) const
	{
		if (OutUnavailableReason)
		{
			*OutUnavailableReason = FText::GetEmpty();
		}

		const auto Reject = [OutUnavailableReason](const FText& Reason)
		{
			if (OutUnavailableReason)
			{
				*OutUnavailableReason = Reason;
			}
			return false;
		};

		if (!bInRoom)
		{
			return Reject(NSLOCTEXT("LeftBehind", "InviteOutsideRoom", "친구를 초대하려면 먼저 방에 들어가야 합니다."));
		}
		if (!IsLoggedIn())
		{
			return Reject(NSLOCTEXT("LeftBehind", "InviteRequiresLogin", "친구를 초대하려면 EOS 로그인이 필요합니다."));
		}
		if (!ExternalUI.IsValid())
		{
			return Reject(NSLOCTEXT("LeftBehind", "OverlayUnavailable", "EOS 소셜 오버레이를 사용할 수 없습니다. Overlay Redistributable 설치를 확인해 주세요."));
		}

		bool bOverlayEnabled = false;
		bool bSocialOverlayEnabled = false;
		const TCHAR* EOSSettingsSection = TEXT("/Script/OnlineSubsystemEOS.EOSSettings");
		if (!GConfig
			|| !GConfig->GetBool(EOSSettingsSection, TEXT("bEnableOverlay"), bOverlayEnabled, GEngineIni)
			|| !GConfig->GetBool(EOSSettingsSection, TEXT("bEnableSocialOverlay"), bSocialOverlayEnabled, GEngineIni)
			|| !bOverlayEnabled
			|| !bSocialOverlayEnabled)
		{
			return Reject(NSLOCTEXT(
				"LeftBehind",
				"SocialOverlayDisabled",
				"EOS 친구 오버레이가 꺼져 있습니다. 게임과 에디터를 완전히 종료한 뒤 다시 실행해 주세요."));
		}

#if WITH_EDITOR
		const ULB_OnlineSessionSubsystem* OwnerSubsystem = Owner.Get();
		const UWorld* World = IsValid(OwnerSubsystem) ? OwnerSubsystem->GetWorld() : nullptr;
		if (IsValid(World) && World->WorldType == EWorldType::PIE)
		{
			return Reject(NSLOCTEXT(
				"LeftBehind",
				"SocialOverlayUnavailableInPIE",
				"UE 5.7의 PIE에서는 Epic 친구 오버레이를 열 수 없습니다. 별도 게임 창이나 패키지에서 사용해 주세요."));
		}
#endif

		return true;
	}

	bool LockRoomForRaid()
	{
		return BeginPhaseUpdate(ELBRoomPhase::InRaid, false);
	}

	bool ReopenRoomAfterRaid()
	{
		return BeginPhaseUpdate(ELBRoomPhase::Waiting, false);
	}

	bool IsInRoom() const
	{
		return bInRoom && Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession) != nullptr;
	}

	bool IsRoomHost() const
	{
		const FNamedOnlineSession* NamedSession = Sessions.IsValid()
			? Sessions->GetNamedSession(NAME_GameSession)
			: nullptr;
		return bInRoom && bRoomHost && NamedSession && NamedSession->bHosting;
	}

private:
	TWeakObjectPtr<ULB_OnlineSessionSubsystem> Owner;
	IOnlineSubsystem* OnlineSubsystem = nullptr;
	IOnlineIdentityPtr Identity;
	IOnlineSessionPtr Sessions;
	IOnlineExternalUIPtr ExternalUI;
	TSharedPtr<FOnlineSessionSearch> ActiveSearch;
	TMap<FString, int32> RoomResultIndices;

	ELBPendingOnlineOperation PendingOperation = ELBPendingOnlineOperation::None;
	ELBRoomPhase CurrentRoomPhase = ELBRoomPhase::Waiting;
	ELBRoomPhase PendingRoomPhase = ELBRoomPhase::Waiting;
	bool bInRoom = false;
	bool bRoomHost = false;
	bool bConnectionTravelPending = false;
	bool bMenuTravelPending = false;
	bool bTravelToMenuAfterDestroy = false;
	FText ErrorAfterDestroy;

	FDelegateHandle LoginCompleteHandle;
	FDelegateHandle CreateCompleteHandle;
	FDelegateHandle FindCompleteHandle;
	FDelegateHandle JoinCompleteHandle;
	FDelegateHandle DestroyCompleteHandle;
	FDelegateHandle UpdateCompleteHandle;
	FDelegateHandle InviteAcceptedHandle;
	FDelegateHandle SessionFailureHandle;
	FDelegateHandle PostLoadMapHandle;
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;

	bool IsLoggedIn() const
	{
		return Identity.IsValid() && Identity->GetLoginStatus(LBLocalUserNum) == ELoginStatus::LoggedIn;
	}

	bool IsSessionOwnedByLocalUser(const FOnlineSessionSearchResult& SessionResult) const
	{
		const FUniqueNetIdPtr LocalUserId = Identity.IsValid()
			? Identity->GetUniquePlayerId(LBLocalUserNum)
			: nullptr;
		return LocalUserId.IsValid()
			&& SessionResult.Session.OwningUserId.IsValid()
			&& *LocalUserId == *SessionResult.Session.OwningUserId;
	}

	void ClearLastError() const
	{
		if (ULB_OnlineSessionSubsystem* OwnerSubsystem = Owner.Get())
		{
			OwnerSubsystem->LastError = FText::GetEmpty();
		}
	}

	void ReportError(const FText& ErrorMessage, const ELBOnlineState RecoveryState) const
	{
		if (ULB_OnlineSessionSubsystem* OwnerSubsystem = Owner.Get())
		{
			OwnerSubsystem->LastError = ErrorMessage;
			OwnerSubsystem->SetState(RecoveryState, ErrorMessage);
		}
		UE_LOG(LogLBOnlineSession, Error, TEXT("%s"), *ErrorMessage.ToString());
	}

	void ReportNonFatalError(const FText& ErrorMessage) const
	{
		const ULB_OnlineSessionSubsystem* OwnerSubsystem = Owner.Get();
		ReportError(ErrorMessage, OwnerSubsystem ? OwnerSubsystem->State : ELBOnlineState::Error);
	}

	bool BeginOperation(const ELBPendingOnlineOperation Operation, const ELBOnlineState NewState)
	{
		ULB_OnlineSessionSubsystem* OwnerSubsystem = Owner.Get();
		if (!OwnerSubsystem)
		{
			return false;
		}

		if (!LBOnlineSessionPolicy::CanStartExclusiveOperation(
			PendingOperation != ELBPendingOnlineOperation::None,
			bConnectionTravelPending,
			bMenuTravelPending))
		{
			ReportNonFatalError(NSLOCTEXT("LeftBehind", "OnlineOperationBusy", "다른 온라인 작업이 진행 중입니다. 잠시 후 다시 시도해 주세요."));
			return false;
		}

		PendingOperation = Operation;
		ClearLastError();
		OwnerSubsystem->SetState(NewState);
		return true;
	}

	bool CanUseReadyServices(const FText& Action)
	{
		ULB_OnlineSessionSubsystem* OwnerSubsystem = Owner.Get();
		if (!OwnerSubsystem || !Identity.IsValid() || !Sessions.IsValid())
		{
			ReportError(MakeOnlineError(Action, TEXT("EOS interface unavailable")), ELBOnlineState::Error);
			return false;
		}

		if (!IsLoggedIn())
		{
			ReportError(
				FText::Format(NSLOCTEXT("LeftBehind", "SignInFirst", "{0} 전에 EOS 로그인이 필요합니다."), Action),
				ELBOnlineState::SignedOut);
			return false;
		}

		if (OwnerSubsystem->State != ELBOnlineState::Ready)
		{
			ReportNonFatalError(NSLOCTEXT("LeftBehind", "OnlineNotReady", "지금은 이 작업을 시작할 수 없습니다. 진행 중인 작업이 끝날 때까지 기다려 주세요."));
			return false;
		}

		return true;
	}

	void HandleLoginComplete(
		const int32 LocalUserNum,
		const bool bWasSuccessful,
		const FUniqueNetId& UserId,
		const FString& Error)
	{
		(void)UserId;
		ClearLoginDelegate();
		PendingOperation = ELBPendingOnlineOperation::None;
		if (LocalUserNum != LBLocalUserNum || !bWasSuccessful || !IsLoggedIn())
		{
			ReportError(MakeOnlineError(NSLOCTEXT("LeftBehind", "SignInAction", "EOS 로그인"), Error), ELBOnlineState::Error);
			return;
		}

		ClearLastError();
		if (ULB_OnlineSessionSubsystem* OwnerSubsystem = Owner.Get())
		{
			OwnerSubsystem->SetState(ELBOnlineState::Ready);
		}
	}

	void HandleCreateComplete(const FName SessionName, const bool bWasSuccessful)
	{
		ClearCreateDelegate();
		PendingOperation = ELBPendingOnlineOperation::None;
		if (SessionName != NAME_GameSession || !bWasSuccessful || !Sessions.IsValid())
		{
			if (Sessions.IsValid())
			{
				Sessions->RemoveNamedSession(NAME_GameSession);
			}
			ReportError(MakeOnlineError(NSLOCTEXT("LeftBehind", "CreateRoomAction", "방 만들기"), FString()), ELBOnlineState::Ready);
			return;
		}

		FNamedOnlineSession* NamedSession = Sessions->GetNamedSession(NAME_GameSession);
		if (!NamedSession)
		{
			ReportError(
				NSLOCTEXT("LeftBehind", "CreatedRoomMissing", "EOS가 방 만들기를 완료했지만 방 정보를 찾지 못했습니다."),
				ELBOnlineState::Ready);
			return;
		}

		bInRoom = true;
		bRoomHost = true;
		CurrentRoomPhase = ELBRoomPhase::Waiting;
		bConnectionTravelPending = true;
		ClearLastError();
		if (ULB_OnlineSessionSubsystem* OwnerSubsystem = Owner.Get())
		{
			OwnerSubsystem->SetRooms({});
			OwnerSubsystem->SetState(ELBOnlineState::Traveling);
			UGameplayStatics::OpenLevel(OwnerSubsystem, LBMainMenuMap, true, TEXT("listen"));
		}
	}

	void HandleFindComplete(const bool bWasSuccessful)
	{
		ClearFindDelegate();
		PendingOperation = ELBPendingOnlineOperation::None;
		ULB_OnlineSessionSubsystem* OwnerSubsystem = Owner.Get();
		if (!OwnerSubsystem)
		{
			return;
		}

		if (!bWasSuccessful || !ActiveSearch.IsValid())
		{
			ActiveSearch.Reset();
			RoomResultIndices.Reset();
			ReportError(MakeOnlineError(NSLOCTEXT("LeftBehind", "RefreshRoomsAction", "방 목록 새로고침"), FString()), ELBOnlineState::Ready);
			return;
		}

		const int32 RawSearchResultCount = ActiveSearch->SearchResults.Num();
		TArray<FLBRoomSummary> NewRooms;
		RoomResultIndices.Reset();
		for (int32 Index = 0; Index < ActiveSearch->SearchResults.Num(); ++Index)
		{
			const FOnlineSessionSearchResult& Result = ActiveSearch->SearchResults[Index];
			if (!Result.IsValid() || Result.Session.SessionSettings.BuildUniqueId != GetBuildUniqueId())
			{
				continue;
			}

			FString Phase;
			if (!Result.Session.SessionSettings.Get(LBOnlineSessionPolicy::GetRoomPhaseKey(), Phase)
				|| Phase != LBOnlineSessionPolicy::GetWaitingPhaseValue())
			{
				continue;
			}

			const FString RoomId = Result.GetSessionIdStr();
			if (RoomId.IsEmpty() || RoomId == TEXT("InvalidSession") || RoomResultIndices.Contains(RoomId))
			{
				continue;
			}

			FLBRoomSummary& Room = NewRooms.AddDefaulted_GetRef();
			Room.RoomId = RoomId;
			Room.HostDisplayName = Result.Session.OwningUserName.IsEmpty()
				? NSLOCTEXT("LeftBehind", "UnknownHost", "이름 없는 호스트").ToString()
				: Result.Session.OwningUserName;
			Room.MaxPlayers = FMath::Max(0, Result.Session.SessionSettings.NumPublicConnections);
			Room.CurrentPlayers = FMath::Clamp(
				Room.MaxPlayers - Result.Session.NumOpenPublicConnections,
				0,
				Room.MaxPlayers);
			// OnlineSubsystemEOS does not restore bShouldAdvertise in CopyLobbyData.
			// Waiting phase plus an open public slot is the authoritative check.
			Room.bCanJoin = Room.MaxPlayers > 0
				&& Result.Session.NumOpenPublicConnections > 0;
			RoomResultIndices.Add(RoomId, Index);
		}

		NewRooms.Sort([](const FLBRoomSummary& Left, const FLBRoomSummary& Right)
		{
			const int32 NameOrder = Left.HostDisplayName.Compare(Right.HostDisplayName, ESearchCase::IgnoreCase);
			return NameOrder == 0 ? Left.RoomId < Right.RoomId : NameOrder < 0;
		});
		UE_LOG(
			LogLBOnlineSession,
			Log,
			TEXT("EOS lobby search completed. RawResults=%d PublishedRooms=%d"),
			RawSearchResultCount,
			NewRooms.Num());

		ClearLastError();
		OwnerSubsystem->SetRooms(MoveTemp(NewRooms));
		OwnerSubsystem->SetState(ELBOnlineState::Ready);
	}

	bool BeginJoin(FOnlineSessionSearchResult DesiredSession)
	{
		// 서로 다른 PIE 인스턴스라도 같은 Epic 계정이면 EOS P2P 목적지가 자기 자신이 되어
		// handshake가 응답 없이 timeout된다. 네트워크 이동 전에 명확하게 차단한다.
		if (IsSessionOwnedByLocalUser(DesiredSession))
		{
			ReportNonFatalError(NSLOCTEXT(
				"LeftBehind",
				"JoinOwnEOSAccountRoom",
				"같은 Epic 계정으로 만든 방에는 입장할 수 없습니다. 두 번째 PIE 인스턴스가 Play Credentials의 Player2를 사용하게 설정하세요."));
			return false;
		}

		if (!BeginOperation(ELBPendingOnlineOperation::Join, ELBOnlineState::Joining))
		{
			return false;
		}

		// EOS does not restore these bits when it copies lobby search/invite data.
		DesiredSession.Session.SessionSettings.bUsesPresence = true;
		DesiredSession.Session.SessionSettings.bUseLobbiesIfAvailable = true;
		JoinCompleteHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
			FOnJoinSessionCompleteDelegate::CreateSP(
				AsShared(),
				&FLBOnlineSessionRuntime::HandleJoinComplete));
		if (!Sessions->JoinSession(LBLocalUserNum, NAME_GameSession, DesiredSession))
		{
			ClearJoinDelegate();
			PendingOperation = ELBPendingOnlineOperation::None;
			ReportError(MakeOnlineError(NSLOCTEXT("LeftBehind", "JoinRoomAction", "방 입장"), FString()), ELBOnlineState::Ready);
			return false;
		}

		return true;
	}

	void HandleJoinComplete(const FName SessionName, const EOnJoinSessionCompleteResult::Type Result)
	{
		ClearJoinDelegate();
		PendingOperation = ELBPendingOnlineOperation::None;
		if (SessionName != NAME_GameSession || Result != EOnJoinSessionCompleteResult::Success || !Sessions.IsValid())
		{
			ReportError(
				MakeOnlineError(NSLOCTEXT("LeftBehind", "JoinRoomAction", "방 입장"), LexToString(Result)),
				ELBOnlineState::Ready);
			return;
		}

		FString ConnectString;
		if (!Sessions->GetResolvedConnectString(NAME_GameSession, ConnectString) || ConnectString.IsEmpty())
		{
			bInRoom = true;
			bRoomHost = false;
			PendingOperation = ELBPendingOnlineOperation::Leave;
			if (ULB_OnlineSessionSubsystem* OwnerSubsystem = Owner.Get())
			{
				OwnerSubsystem->SetState(ELBOnlineState::Leaving);
			}
			BeginDestroySession(
				false,
				NSLOCTEXT("LeftBehind", "ConnectStringMissing", "방에는 참가했지만 호스트의 접속 주소를 받지 못했습니다. 다시 시도해 주세요."));
			return;
		}
		ULB_OnlineSessionSubsystem* OwnerSubsystem = Owner.Get();
		APlayerController* PlayerController = OwnerSubsystem && OwnerSubsystem->GetWorld()
			? OwnerSubsystem->GetWorld()->GetFirstPlayerController()
			: nullptr;
		if (!IsValid(PlayerController))
		{
			bInRoom = true;
			bRoomHost = false;
			PendingOperation = ELBPendingOnlineOperation::Leave;
			if (OwnerSubsystem)
			{
				OwnerSubsystem->SetState(ELBOnlineState::Leaving);
			}
			BeginDestroySession(
				false,
				NSLOCTEXT("LeftBehind", "LocalControllerMissing", "방 이동에 필요한 로컬 플레이어를 찾지 못했습니다. 다시 시도해 주세요."));
			return;
		}

		bInRoom = true;
		bRoomHost = false;
		CurrentRoomPhase = ELBRoomPhase::Waiting;
		bConnectionTravelPending = true;
		ClearLastError();
		OwnerSubsystem->SetState(ELBOnlineState::Traveling);
		PlayerController->ClientTravel(ConnectString, TRAVEL_Absolute);
	}

	bool BeginDestroySession(const bool bInTravelToMenu, const FText& InErrorAfterDestroy)
	{
		bTravelToMenuAfterDestroy = bInTravelToMenu;
		ErrorAfterDestroy = InErrorAfterDestroy;

		if (!Sessions->GetNamedSession(NAME_GameSession))
		{
			HandleDestroyComplete(NAME_GameSession, true);
			return true;
		}

		DestroyCompleteHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateSP(
				AsShared(),
				&FLBOnlineSessionRuntime::HandleDestroyComplete));
		if (!Sessions->DestroySession(NAME_GameSession))
		{
			ClearDestroyDelegate();
			PendingOperation = ELBPendingOnlineOperation::None;
			ReportError(
				MakeOnlineError(NSLOCTEXT("LeftBehind", "LeaveRoomAction", "방 나가기"), FString()),
				bInRoom ? ELBOnlineState::InRoom : ELBOnlineState::Error);
			return false;
		}

		return true;
	}

	void HandleDestroyComplete(const FName SessionName, const bool bWasSuccessful)
	{
		ClearDestroyDelegate();
		PendingOperation = ELBPendingOnlineOperation::None;
		if (SessionName != NAME_GameSession || !bWasSuccessful)
		{
			ReportError(
				MakeOnlineError(NSLOCTEXT("LeftBehind", "LeaveRoomAction", "방 나가기"), FString()),
				bInRoom ? ELBOnlineState::InRoom : ELBOnlineState::Error);
			return;
		}

		bInRoom = false;
		bRoomHost = false;
		CurrentRoomPhase = ELBRoomPhase::Waiting;
		bConnectionTravelPending = false;
		RoomResultIndices.Reset();
		ActiveSearch.Reset();

		ULB_OnlineSessionSubsystem* OwnerSubsystem = Owner.Get();
		if (!OwnerSubsystem)
		{
			return;
		}
		OwnerSubsystem->SetRooms({});

		if (!ErrorAfterDestroy.IsEmpty())
		{
			const FText DeferredError = ErrorAfterDestroy;
			ErrorAfterDestroy = FText::GetEmpty();
			bTravelToMenuAfterDestroy = false;
			ReportError(DeferredError, ELBOnlineState::Error);
			return;
		}

		if (bTravelToMenuAfterDestroy)
		{
			bTravelToMenuAfterDestroy = false;
			bMenuTravelPending = true;
			ClearLastError();
			OwnerSubsystem->SetState(ELBOnlineState::Traveling);
			UGameplayStatics::OpenLevel(OwnerSubsystem, LBMainMenuMap, true);
			return;
		}

		ClearLastError();
		OwnerSubsystem->SetState(IsLoggedIn() ? ELBOnlineState::Ready : ELBOnlineState::SignedOut);
	}

	bool BeginPhaseUpdate(const ELBRoomPhase NewPhase, const bool bAutomatic)
	{
		ULB_OnlineSessionSubsystem* OwnerSubsystem = Owner.Get();
		FNamedOnlineSession* NamedSession = Sessions.IsValid()
			? Sessions->GetNamedSession(NAME_GameSession)
			: nullptr;
		if (!OwnerSubsystem)
		{
			return false;
		}

		if (!bInRoom || !bRoomHost || !NamedSession || !NamedSession->bHosting)
		{
			const FText Error = NSLOCTEXT("LeftBehind", "OnlyHostCanUpdateRoom", "방장만 방의 레이드 상태를 변경할 수 있습니다.");
			ReportNonFatalError(Error);
			OwnerSubsystem->OnRoomPhaseUpdateComplete.Broadcast(false, NewPhase, Error);
			return false;
		}

		if (CurrentRoomPhase == NewPhase)
		{
			ClearLastError();
			OwnerSubsystem->SetState(NewPhase == ELBRoomPhase::InRaid ? ELBOnlineState::Traveling : ELBOnlineState::InRoom);
			OwnerSubsystem->OnRoomPhaseUpdateComplete.Broadcast(true, NewPhase, FText::GetEmpty());
			return true;
		}

		if (!LBOnlineSessionPolicy::CanStartExclusiveOperation(
			PendingOperation != ELBPendingOnlineOperation::None,
			bConnectionTravelPending,
			bMenuTravelPending))
		{
			const FText Error = bAutomatic
				? NSLOCTEXT("LeftBehind", "AutoReopenBusy", "대기실을 다시 여는 동안 다른 온라인 작업과 충돌했습니다. 방장이 다시 시도해 주세요.")
				: NSLOCTEXT("LeftBehind", "PhaseUpdateBusy", "다른 온라인 작업이 진행 중이라 방 상태를 바꿀 수 없습니다.");
			ReportNonFatalError(Error);
			OwnerSubsystem->OnRoomPhaseUpdateComplete.Broadcast(false, NewPhase, Error);
			return false;
		}

		FOnlineSessionSettings UpdatedSettings = NamedSession->SessionSettings;
		if (NewPhase == ELBRoomPhase::InRaid)
		{
			const int32 TotalCapacity = NamedSession->SessionSettings.NumPublicConnections
				+ NamedSession->SessionSettings.NumPrivateConnections;
			const int32 OpenConnections = NamedSession->NumOpenPublicConnections
				+ NamedSession->NumOpenPrivateConnections;
			const int32 TrackedMembers = FMath::Max(
				NamedSession->RegisteredPlayers.Num(),
				NamedSession->SessionSettings.MemberSettings.Num());
			const int32 CurrentPlayers = FMath::Clamp(
				FMath::Max(TrackedMembers, TotalCapacity - OpenConnections),
				1,
				LBOnlineSessionPolicy::MaxPublicConnections);
			LBOnlineSessionPolicy::ApplyInRaidPolicy(UpdatedSettings, CurrentPlayers);
		}
		else
		{
			LBOnlineSessionPolicy::ApplyWaitingPolicy(UpdatedSettings);
		}

		PendingOperation = ELBPendingOnlineOperation::UpdatePhase;
		PendingRoomPhase = NewPhase;
		ClearLastError();
		OwnerSubsystem->SetState(ELBOnlineState::Traveling);
		UpdateCompleteHandle = Sessions->AddOnUpdateSessionCompleteDelegate_Handle(
			FOnUpdateSessionCompleteDelegate::CreateSP(
				AsShared(),
				&FLBOnlineSessionRuntime::HandleUpdateComplete));
		if (!Sessions->UpdateSession(NAME_GameSession, UpdatedSettings, true))
		{
			ClearUpdateDelegate();
			PendingOperation = ELBPendingOnlineOperation::None;
			const FText Error = MakeOnlineError(NSLOCTEXT("LeftBehind", "UpdateRoomAction", "방 상태 변경"), FString());
			ReportError(Error, ELBOnlineState::InRoom);
			OwnerSubsystem->OnRoomPhaseUpdateComplete.Broadcast(false, NewPhase, Error);
			return false;
		}

		return true;
	}

	void HandleUpdateComplete(const FName SessionName, const bool bWasSuccessful)
	{
		ClearUpdateDelegate();
		PendingOperation = ELBPendingOnlineOperation::None;
		ULB_OnlineSessionSubsystem* OwnerSubsystem = Owner.Get();
		if (!OwnerSubsystem)
		{
			return;
		}

		if (SessionName != NAME_GameSession || !bWasSuccessful)
		{
			const FText Error = MakeOnlineError(NSLOCTEXT("LeftBehind", "UpdateRoomAction", "방 상태 변경"), FString());
			ReportError(Error, ELBOnlineState::InRoom);
			OwnerSubsystem->OnRoomPhaseUpdateComplete.Broadcast(false, PendingRoomPhase, Error);
			return;
		}

		CurrentRoomPhase = PendingRoomPhase;
		ClearLastError();
		OwnerSubsystem->SetState(CurrentRoomPhase == ELBRoomPhase::InRaid
			? ELBOnlineState::Traveling
			: ELBOnlineState::InRoom);
		OwnerSubsystem->OnRoomPhaseUpdateComplete.Broadcast(true, CurrentRoomPhase, FText::GetEmpty());
	}

	void HandleInviteAccepted(
		const bool bWasSuccessful,
		const int32 ControllerId,
		FUniqueNetIdPtr UserId,
		const FOnlineSessionSearchResult& InviteResult)
	{
		(void)UserId;
		if (!bWasSuccessful || ControllerId != LBLocalUserNum || !InviteResult.IsValid())
		{
			ReportNonFatalError(NSLOCTEXT("LeftBehind", "InvalidInvite", "친구 초대 정보를 불러오지 못했습니다. 새 초대를 받아 다시 시도해 주세요."));
			return;
		}

		ULB_OnlineSessionSubsystem* OwnerSubsystem = Owner.Get();
		if (!OwnerSubsystem || bInRoom || PendingOperation != ELBPendingOnlineOperation::None
			|| OwnerSubsystem->State == ELBOnlineState::Traveling)
		{
			ReportNonFatalError(NSLOCTEXT("LeftBehind", "InviteWhileBusy", "현재 방이나 레이드를 자동으로 나가지 않았습니다. 먼저 나간 뒤 초대를 다시 수락해 주세요."));
			return;
		}

		if (!IsLoggedIn())
		{
			ReportError(NSLOCTEXT("LeftBehind", "InviteRequiresLogin", "초대를 수락하려면 먼저 EOS에 로그인해야 합니다."), ELBOnlineState::SignedOut);
			return;
		}

		if (Sessions->GetNamedSession(NAME_GameSession))
		{
			ReportNonFatalError(NSLOCTEXT("LeftBehind", "InviteNamedSessionExists", "이전 방 정보가 아직 정리되지 않았습니다. 방 나가기를 누른 뒤 초대를 다시 수락해 주세요."));
			return;
		}

		if (!LBOnlineSessionPolicy::CanAcceptInvite(
			InviteResult.Session.SessionSettings,
			InviteResult.Session.NumOpenPublicConnections,
			GetBuildUniqueId()))
		{
			ReportNonFatalError(NSLOCTEXT(
				"LeftBehind",
				"InviteRoomUnavailable",
				"초대한 방이 이미 레이드 중이거나 가득 찼거나 현재 게임 빌드와 맞지 않습니다. 방장이 대기실로 돌아온 뒤 새 초대를 보내야 합니다."));
			return;
		}

		OwnerSubsystem->SetState(ELBOnlineState::Ready);
		FOnlineSessionSearchResult InviteCopy = InviteResult;
		InviteCopy.Session.SessionSettings.bUsesPresence = true;
		InviteCopy.Session.SessionSettings.bUseLobbiesIfAvailable = true;
		BeginJoin(MoveTemp(InviteCopy));
	}

	void HandleSessionFailure(const FUniqueNetId& PlayerId, const ESessionFailure::Type FailureType)
	{
		(void)PlayerId;
		UE_LOG(LogLBOnlineSession, Error, TEXT("EOS session failure: %s"), LexToString(FailureType));
		ResetAfterFatalConnectionFailure(
			NSLOCTEXT("LeftBehind", "SessionConnectionLost", "방 연결이 끊어졌습니다. 방장이 종료했거나 네트워크 연결이 불안정할 수 있습니다."));
	}

	void HandleNetworkFailure(
		UWorld* FailedWorld,
		UNetDriver* NetDriver,
		const ENetworkFailure::Type FailureType,
		const FString& ErrorString)
	{
		if (!IsNetworkFailureForOwner(FailedWorld, NetDriver) || !IsOnlineConnectionActive())
		{
			return;
		}

		const bool bOnlyFatalForClient = FailureType == ENetworkFailure::ConnectionLost
			|| FailureType == ENetworkFailure::ConnectionTimeout
			|| FailureType == ENetworkFailure::NetGuidMismatch
			|| FailureType == ENetworkFailure::NetChecksumMismatch;
		if (bOnlyFatalForClient && NetDriver->GetNetMode() != NM_Client)
		{
			// A listen server broadcasts these when one remote client disconnects.
			// That client departure must not destroy the host's EOS lobby.
			return;
		}

		UE_LOG(
			LogLBOnlineSession,
			Error,
			TEXT("Network failure. Type=%s Detail=%s"),
			ENetworkFailure::ToString(FailureType),
			*ErrorString);
		ResetAfterFatalConnectionFailure(
			FText::Format(
				NSLOCTEXT("LeftBehind", "NetworkConnectionLost", "방 연결이 끊어졌습니다 ({0}). 메인 화면에서 다시 시도해 주세요."),
				FText::FromString(ENetworkFailure::ToString(FailureType))));
	}

	void HandleTravelFailure(
		UWorld* FailedWorld,
		const ETravelFailure::Type FailureType,
		const FString& ErrorString)
	{
		if (!IsFailureForOwner(FailedWorld) || !IsOnlineConnectionActive())
		{
			return;
		}

		UE_LOG(
			LogLBOnlineSession,
			Error,
			TEXT("Travel failure. Type=%s Detail=%s"),
			ETravelFailure::ToString(FailureType),
			*ErrorString);
		ResetAfterFatalConnectionFailure(
			FText::Format(
				NSLOCTEXT("LeftBehind", "OnlineTravelFailed", "방으로 이동하지 못했습니다 ({0}). 메인 화면에서 다시 시도해 주세요."),
				FText::FromString(ETravelFailure::ToString(FailureType))));
	}

	bool IsFailureForOwner(const UWorld* FailedWorld) const
	{
		const ULB_OnlineSessionSubsystem* OwnerSubsystem = Owner.Get();
		return OwnerSubsystem
			&& (!FailedWorld || FailedWorld->GetGameInstance() == OwnerSubsystem->GetGameInstance());
	}

	bool IsNetworkFailureForOwner(const UWorld* FailedWorld, const UNetDriver* NetDriver) const
	{
		const ULB_OnlineSessionSubsystem* OwnerSubsystem = Owner.Get();
		if (!OwnerSubsystem || !NetDriver)
		{
			return false;
		}

		if (NetDriver->NetDriverName != NAME_GameNetDriver
			&& NetDriver->NetDriverName != NAME_PendingNetDriver)
		{
			return false;
		}

		if (FailedWorld)
		{
			return FailedWorld->GetGameInstance() == OwnerSubsystem->GetGameInstance();
		}

		const FWorldContext* PendingContext = GEngine
			? GEngine->GetWorldContextFromPendingNetGameNetDriver(NetDriver)
			: nullptr;
		return PendingContext && PendingContext->OwningGameInstance == OwnerSubsystem->GetGameInstance();
	}

	bool IsOnlineConnectionActive() const
	{
		return bInRoom
			|| bConnectionTravelPending
			|| bMenuTravelPending
			|| PendingOperation != ELBPendingOnlineOperation::None
			|| (Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession));
	}

	void ResetAfterFatalConnectionFailure(const FText& Error)
	{
		ClearOperationDelegates();
		if (Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession))
		{
			const EOnlineSessionState::Type SessionState = Sessions->GetSessionState(NAME_GameSession);
			if (SessionState != EOnlineSessionState::Destroying)
			{
				const TWeakPtr<FLBOnlineSessionRuntime> WeakRuntime = AsShared();
				const bool bDestroyStarted = Sessions->DestroySession(
					NAME_GameSession,
					FOnDestroySessionCompleteDelegate::CreateLambda(
						[WeakRuntime](const FName SessionName, const bool bWasSuccessful)
						{
							if (const TSharedPtr<FLBOnlineSessionRuntime> This = WeakRuntime.Pin())
							{
								UE_LOG(
									LogLBOnlineSession,
									Log,
									TEXT("Failure cleanup completed. Session=%s Success=%d"),
									*SessionName.ToString(),
									bWasSuccessful ? 1 : 0);
								if (This->Sessions.IsValid() && This->Sessions->GetNamedSession(SessionName))
								{
									This->Sessions->RemoveNamedSession(SessionName);
								}
							}
						}));
				if (!bDestroyStarted)
				{
					Sessions->RemoveNamedSession(NAME_GameSession);
				}
			}
		}
		bInRoom = false;
		bRoomHost = false;
		bConnectionTravelPending = false;
		bMenuTravelPending = false;
		CurrentRoomPhase = ELBRoomPhase::Waiting;
		ActiveSearch.Reset();
		RoomResultIndices.Reset();
		ReportError(Error, ELBOnlineState::Error);
		if (ULB_OnlineSessionSubsystem* OwnerSubsystem = Owner.Get())
		{
			OwnerSubsystem->SetRooms({});
		}
		// UEngine owns disconnect recovery travel. Starting another OpenLevel here can
		// race CallHandleDisconnectForFailure and hide the original failure reason.
	}

	void HandlePostLoadMap(UWorld* LoadedWorld)
	{
		ULB_OnlineSessionSubsystem* OwnerSubsystem = Owner.Get();
		if (!OwnerSubsystem || !IsValid(LoadedWorld) || LoadedWorld->GetGameInstance() != OwnerSubsystem->GetGameInstance())
		{
			return;
		}

		if (bMenuTravelPending)
		{
			bMenuTravelPending = false;
			ClearLastError();
			OwnerSubsystem->SetState(IsLoggedIn() ? ELBOnlineState::Ready : ELBOnlineState::SignedOut);
			return;
		}

		if (bConnectionTravelPending)
		{
			bConnectionTravelPending = false;
			ClearLastError();
			OwnerSubsystem->SetState(ELBOnlineState::InRoom);
			return;
		}

		const FString LoadedMapName = UWorld::RemovePIEPrefix(
			FPackageName::GetShortName(LoadedWorld->GetOutermost()->GetName()));
		if (bInRoom
			&& bRoomHost
			&& CurrentRoomPhase == ELBRoomPhase::InRaid
			&& LoadedMapName == FPackageName::GetShortName(LBMainMenuMap.ToString()))
		{
			BeginPhaseUpdate(ELBRoomPhase::Waiting, true);
		}
	}

	void ClearLoginDelegate()
	{
		if (Identity.IsValid() && LoginCompleteHandle.IsValid())
		{
			Identity->ClearOnLoginCompleteDelegate_Handle(LBLocalUserNum, LoginCompleteHandle);
		}
		LoginCompleteHandle.Reset();
	}

	void ClearCreateDelegate()
	{
		if (Sessions.IsValid() && CreateCompleteHandle.IsValid())
		{
			Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateCompleteHandle);
		}
		CreateCompleteHandle.Reset();
	}

	void ClearFindDelegate()
	{
		if (Sessions.IsValid() && FindCompleteHandle.IsValid())
		{
			Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindCompleteHandle);
		}
		FindCompleteHandle.Reset();
	}

	void ClearJoinDelegate()
	{
		if (Sessions.IsValid() && JoinCompleteHandle.IsValid())
		{
			Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinCompleteHandle);
		}
		JoinCompleteHandle.Reset();
	}

	void ClearDestroyDelegate()
	{
		if (Sessions.IsValid() && DestroyCompleteHandle.IsValid())
		{
			Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyCompleteHandle);
		}
		DestroyCompleteHandle.Reset();
	}

	void ClearUpdateDelegate()
	{
		if (Sessions.IsValid() && UpdateCompleteHandle.IsValid())
		{
			Sessions->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateCompleteHandle);
		}
		UpdateCompleteHandle.Reset();
	}

	void ClearOperationDelegates()
	{
		ClearLoginDelegate();
		ClearCreateDelegate();
		ClearFindDelegate();
		ClearJoinDelegate();
		ClearDestroyDelegate();
		ClearUpdateDelegate();
		PendingOperation = ELBPendingOnlineOperation::None;
	}
};

void ULB_OnlineSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Runtime = MakeShared<FLBOnlineSessionRuntime>(this);
	Runtime->Initialize();
}

void ULB_OnlineSessionSubsystem::Deinitialize()
{
	if (Runtime.IsValid())
	{
		Runtime->Shutdown();
		Runtime.Reset();
	}

	Super::Deinitialize();
}

bool ULB_OnlineSessionSubsystem::SignIn()
{
	return Runtime.IsValid() && Runtime->SignIn();
}

bool ULB_OnlineSessionSubsystem::CreateRoom()
{
	return Runtime.IsValid() && Runtime->CreateRoom();
}

bool ULB_OnlineSessionSubsystem::RefreshRooms()
{
	return Runtime.IsValid() && Runtime->RefreshRooms();
}

bool ULB_OnlineSessionSubsystem::JoinRoom(const FString& RoomId)
{
	return Runtime.IsValid() && Runtime->JoinRoom(RoomId);
}

bool ULB_OnlineSessionSubsystem::LeaveRoom()
{
	return Runtime.IsValid() && Runtime->LeaveRoom();
}

bool ULB_OnlineSessionSubsystem::OpenSocialOverlay()
{
	return Runtime.IsValid() && Runtime->OpenSocialOverlay();
}

bool ULB_OnlineSessionSubsystem::CanOpenSocialOverlay(FText* OutUnavailableReason) const
{
	if (Runtime.IsValid())
	{
		return Runtime->CanOpenSocialOverlay(OutUnavailableReason);
	}

	if (OutUnavailableReason)
	{
		*OutUnavailableReason = NSLOCTEXT(
			"LeftBehind",
			"SocialOverlayRuntimeUnavailable",
			"EOS 친구 오버레이가 아직 준비되지 않았습니다.");
	}
	return false;
}

bool ULB_OnlineSessionSubsystem::LockRoomForRaid()
{
	return Runtime.IsValid() && Runtime->LockRoomForRaid();
}

bool ULB_OnlineSessionSubsystem::ReopenRoomAfterRaid()
{
	return Runtime.IsValid() && Runtime->ReopenRoomAfterRaid();
}

bool ULB_OnlineSessionSubsystem::IsInRoom() const
{
	return Runtime.IsValid() && Runtime->IsInRoom();
}

bool ULB_OnlineSessionSubsystem::IsRoomHost() const
{
	return Runtime.IsValid() && Runtime->IsRoomHost();
}

void ULB_OnlineSessionSubsystem::SetState(const ELBOnlineState NewState, const FText& StatusMessage)
{
	State = NewState;
	OnStateChanged.Broadcast(State, StatusMessage);
}

void ULB_OnlineSessionSubsystem::SetRooms(TArray<FLBRoomSummary>&& NewRooms)
{
	Rooms = MoveTemp(NewRooms);
	OnRoomsChanged.Broadcast(Rooms);
}
