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
