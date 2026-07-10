#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "LBMainMenuAssetMigrationCommandlet.generated.h"

UCLASS()
class ULBMainMenuAssetMigrationCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	ULBMainMenuAssetMigrationCommandlet();
	virtual int32 Main(const FString& Params) override;
};
