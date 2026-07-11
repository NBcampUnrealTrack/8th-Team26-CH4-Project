#pragma once

#include "CoreMinimal.h"
#include "OnlineSessionSettings.h"
#include "System/Online/LB_OnlineSessionSubsystem.h"

namespace LBOnlineSessionPolicy
{
	enum class ETransportMode : uint8
	{
		EOS,
		EditorLan,
		Unsupported
	};

	inline constexpr int32 MaxPublicConnections = 4;
	inline constexpr int32 MaxSearchResults = 50;

	FName GetRoomPhaseKey();
	FString GetWaitingPhaseValue();
	FString GetInRaidPhaseValue();
	ETransportMode ResolveTransportMode(FName SubsystemName, bool bAllowEditorLanFallback);

	FOnlineSessionSettings MakeWaitingRoomSettings(bool bUseLan = false);
	void ApplyWaitingPolicy(FOnlineSessionSettings& Settings, bool bUseLan = false);
	void ApplyInRaidPolicy(FOnlineSessionSettings& Settings, int32 CurrentPlayers, bool bUseLan = false);

	bool CanStartExclusiveOperation(bool bHasPendingOperation, bool bConnectionTravelPending, bool bMenuTravelPending);
	bool CanAcceptInvite(
		const FOnlineSessionSettings& InviteSettings,
		int32 NumOpenPublicConnections,
		int32 ExpectedBuildUniqueId);
	bool IsCurrentJoinSelection(const TArray<FLBRoomSummary>& Rooms, const FString& RoomId);
}
