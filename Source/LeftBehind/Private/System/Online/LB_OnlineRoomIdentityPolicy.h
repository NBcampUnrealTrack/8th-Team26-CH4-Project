#pragma once

#include "CoreMinimal.h"
#include "OnlineSessionSettings.h"

namespace LBOnlineRoomIdentityPolicy
{
	inline const FName& GetHostCodenameKey()
	{
		static const FName HostCodenameKey(TEXT("HOST_CODENAME"));
		return HostCodenameKey;
	}

	inline void AdvertiseHostCodename(
		FOnlineSessionSettings& Settings,
		const FString& HostCodename)
	{
		FString SanitizedCodename = HostCodename.TrimStartAndEnd();
		if (SanitizedCodename.IsEmpty())
		{
			return;
		}

		Settings.Set(
			GetHostCodenameKey(),
			SanitizedCodename,
			EOnlineDataAdvertisementType::ViaOnlineService);
	}

	inline FString ResolveHostDisplayName(
		const FOnlineSessionSettings& Settings,
		const FString& PlatformDisplayName,
		const FString& UnknownHostFallback)
	{
		FString HostCodename;
		if (Settings.Get(GetHostCodenameKey(), HostCodename))
		{
			HostCodename.TrimStartAndEndInline();
			if (!HostCodename.IsEmpty())
			{
				return HostCodename;
			}
		}

		return PlatformDisplayName.IsEmpty()
			? UnknownHostFallback
			: PlatformDisplayName;
	}
}
