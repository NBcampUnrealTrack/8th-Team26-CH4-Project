#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "LBRaidPauseMenuAssetCommandlet.generated.h"

/** Rebuilds the native-backed raid pause menu Widget Blueprint deterministically. */
UCLASS()
class ULBRaidPauseMenuAssetCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	ULBRaidPauseMenuAssetCommandlet();
	virtual int32 Main(const FString& Params) override;
};
