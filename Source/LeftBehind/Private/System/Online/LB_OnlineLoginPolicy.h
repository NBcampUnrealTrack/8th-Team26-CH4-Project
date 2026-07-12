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

	/**
	 * UE's EOS AutoLogin reads the AUTH_TYPE, AUTH_LOGIN, and AUTH_PASSWORD
	 * command-line arguments. Partial credentials or an empty AUTH_TYPE are
	 * invalid so they cannot silently fall through to another mechanism.
	 */
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
