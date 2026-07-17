#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "System/MainMenu/LBMainMenuTypes.h"

#include "LB_LocalPlayerProfileSubsystem.generated.h"

/**
 * Stores local-player menu choices that must survive non-seamless room travel.
 *
 * This cache is never authoritative for a room. The server-owned PlayerState
 * remains the source of truth after the player connects to a listen server.
 */
UCLASS()
class LEFTBEHIND_API ULB_LocalPlayerProfileSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Validates, sanitizes, and stores a codename for the current game instance. */
	ELBCodenameSubmitResult TrySetCodename(const FString& RawCodename, FString& OutSanitizedCodename);

	bool HasCodename() const { return !Codename.IsEmpty(); }
	const FString& GetCodename() const { return Codename; }
	uint32 GetCodenameRevision() const { return CodenameRevision; }

	void ClearCodename();

private:
	UPROPERTY(Transient)
	FString Codename;

	uint32 CodenameRevision = 0;
};
