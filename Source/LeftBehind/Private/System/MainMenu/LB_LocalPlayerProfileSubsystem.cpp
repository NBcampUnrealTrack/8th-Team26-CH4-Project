#include "System/MainMenu/LB_LocalPlayerProfileSubsystem.h"

#include "GameMode/LB_MainMenuGameMode.h"

ELBCodenameSubmitResult ULB_LocalPlayerProfileSubsystem::TrySetCodename(
	const FString& RawCodename,
	FString& OutSanitizedCodename)
{
	const ELBCodenameSubmitResult Result =
		ALB_MainMenuGameMode::ValidateCodename(RawCodename, OutSanitizedCodename);
	if (Result != ELBCodenameSubmitResult::Accepted)
	{
		return Result;
	}

	if (Codename != OutSanitizedCodename)
	{
		Codename = OutSanitizedCodename;
		++CodenameRevision;
	}
	return ELBCodenameSubmitResult::Accepted;
}

void ULB_LocalPlayerProfileSubsystem::ClearCodename()
{
	if (Codename.IsEmpty())
	{
		return;
	}

	Codename.Reset();
	++CodenameRevision;
}
