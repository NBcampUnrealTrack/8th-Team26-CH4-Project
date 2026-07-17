#pragma once

#include "CoreMinimal.h"
#include "Misc/Parse.h"

namespace LBOnlineLoginPolicy
{
	enum class ELoginRoute : uint8
	{
		AccountPortal,
		AutoLogin,
		Invalid
	};

	inline ELoginRoute SelectLoginRoute(const TCHAR* CommandLine)
	{
		FString AuthType;
		const bool bHasAuthType = FParse::Value(CommandLine, TEXT("AUTH_TYPE="), AuthType);
		FString IgnoredValue;
		const bool bHasAuthLogin = FParse::Value(CommandLine, TEXT("AUTH_LOGIN="), IgnoredValue);
		const bool bHasAuthPassword = FParse::Value(CommandLine, TEXT("AUTH_PASSWORD="), IgnoredValue);
		if (!bHasAuthType)
		{
			return bHasAuthLogin || bHasAuthPassword
				? ELoginRoute::Invalid
				: ELoginRoute::AccountPortal;
		}

		return AuthType.IsEmpty() ? ELoginRoute::Invalid : ELoginRoute::AutoLogin;
	}
}
