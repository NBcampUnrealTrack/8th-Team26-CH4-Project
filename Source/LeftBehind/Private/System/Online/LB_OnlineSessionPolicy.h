#pragma once

#include "CoreMinimal.h"
#include "OnlineSessionSettings.h"
#include "System/Online/LB_OnlineSessionSubsystem.h"

namespace LBOnlineSessionPolicy
{
	inline constexpr int32 MaxPublicConnections = 4;
	inline constexpr int32 MaxSearchResults = 50;

	FName GetRoomPhaseKey();
	FString GetWaitingPhaseValue();
	FString GetInRaidPhaseValue();

	FOnlineSessionSettings MakeWaitingRoomSettings();
	void ApplyWaitingPolicy(FOnlineSessionSettings& Settings);
	void ApplyInRaidPolicy(FOnlineSessionSettings& Settings, int32 CurrentPlayers);

	bool CanStartExclusiveOperation(bool bHasPendingOperation, bool bConnectionTravelPending, bool bMenuTravelPending);
	bool CanAcceptInvite(
		const FOnlineSessionSettings& InviteSettings,
		int32 NumOpenPublicConnections,
		int32 ExpectedBuildUniqueId);
	bool IsCurrentJoinSelection(const TArray<FLBRoomSummary>& Rooms, const FString& RoomId);
}
