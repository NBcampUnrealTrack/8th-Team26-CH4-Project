#include "System/Online/LB_OnlineSessionPolicy.h"

#include "Online/OnlineSessionNames.h"

namespace LBOnlineSessionPolicy
{
	namespace
	{
		const FName RoomPhaseKey(TEXT("ROOM_PHASE"));
		const FString WaitingPhaseValue(TEXT("Waiting"));
		const FString InRaidPhaseValue(TEXT("InRaid"));
	}

	FName GetRoomPhaseKey()
	{
		return RoomPhaseKey;
	}

	FString GetWaitingPhaseValue()
	{
		return WaitingPhaseValue;
	}

	FString GetInRaidPhaseValue()
	{
		return InRaidPhaseValue;
	}

	ETransportMode ResolveTransportMode(
		const FName SubsystemName,
		const bool bAllowEditorLanFallback)
	{
		if (SubsystemName == FName(TEXT("EOS")))
		{
			return ETransportMode::EOS;
		}

		if (bAllowEditorLanFallback && SubsystemName == FName(TEXT("NULL")))
		{
			return ETransportMode::EditorLan;
		}

		return ETransportMode::Unsupported;
	}

	void ApplyTransportPolicy(FOnlineSessionSettings& Settings, const bool bUseLan)
	{
		Settings.bIsLANMatch = bUseLan;
		if (!bUseLan)
		{
			return;
		}

		// The NULL subsystem emulates discovery through LAN beacons. Lobby,
		// presence, and invite flags belong to EOS and must not leak into the
		// editor-only fallback session.
		Settings.bUseLobbiesIfAvailable = false;
		Settings.bUseLobbiesVoiceChatIfAvailable = false;
		Settings.bUsesPresence = false;
		Settings.bAllowJoinViaPresence = false;
		Settings.bAllowJoinViaPresenceFriendsOnly = false;
		Settings.bAllowInvites = false;
	}

	void ApplyWaitingPolicy(FOnlineSessionSettings& Settings, const bool bUseLan)
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
		ApplyTransportPolicy(Settings, bUseLan);
	}

	FOnlineSessionSettings MakeWaitingRoomSettings(const bool bUseLan)
	{
		FOnlineSessionSettings Settings;
		ApplyWaitingPolicy(Settings, bUseLan);
		return Settings;
	}

	void ApplyInRaidPolicy(
		FOnlineSessionSettings& Settings,
		const int32 CurrentPlayers,
		const bool bUseLan)
	{
		Settings.NumPublicConnections = 0;
		Settings.NumPrivateConnections = FMath::Clamp(CurrentPlayers, 1, MaxPublicConnections);
		Settings.bShouldAdvertise = false;
		Settings.bAllowJoinInProgress = false;
		Settings.bAllowInvites = false;
		// Presence remains enabled for the existing members, but no presence join is accepted.
		Settings.bUsesPresence = true;
		Settings.bAllowJoinViaPresence = false;
		Settings.bAllowJoinViaPresenceFriendsOnly = false;
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
		ApplyTransportPolicy(Settings, bUseLan);
		if (bUseLan)
		{
			// NULL treats bIsLANMatch as implicitly advertised. Drop the flag
			// while the raid is locked, then ApplyWaitingPolicy restores it.
			Settings.bIsLANMatch = false;
		}
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
