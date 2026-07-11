#pragma once

#include "OnlineSessionSettings.h"

namespace LBOnlineInvitePolicy
{
	/**
	 * Builds a joinable invite payload without requiring the EOS owner lookup to
	 * have completed. UE 5.7 can return a valid lobby SessionInfo while leaving
	 * OwningUserId empty when the same lobby already exists in CurrentSessionSearch.
	 */
	inline bool PrepareAcceptedInvite(
		const FOnlineSessionSearchResult& InviteResult,
		const TArray<FOnlineSessionSearchResult>& CachedSearchResults,
		FOnlineSessionSearchResult& OutJoinResult)
	{
		OutJoinResult = FOnlineSessionSearchResult();
		if (!InviteResult.IsSessionInfoValid())
		{
			return false;
		}

		OutJoinResult = InviteResult;
		if (OutJoinResult.Session.OwningUserId.IsValid())
		{
			return true;
		}

		const FString InviteSessionId = InviteResult.GetSessionIdStr();
		for (const FOnlineSessionSearchResult& CachedResult : CachedSearchResults)
		{
			if (CachedResult.IsValid()
				&& CachedResult.GetSessionIdStr() == InviteSessionId)
			{
				OutJoinResult.Session.OwningUserId = CachedResult.Session.OwningUserId;
				OutJoinResult.Session.OwningUserName = CachedResult.Session.OwningUserName;
				break;
			}
		}

		return true;
	}
}
