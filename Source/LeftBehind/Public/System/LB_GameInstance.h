#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "LB_GameInstance.generated.h"

class UNetDriver;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLBConnectionFailure, const FText&, ErrorMessage);

/** 월드 교체를 넘어 유지되는 네트워크/여행 실패 상태를 UI에 제공한다. */
UCLASS()
class LEFTBEHIND_API ULB_GameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;
	virtual bool EnableListenServer(bool bEnable, int32 PortOverride = 0) override;

	UPROPERTY(BlueprintAssignable, Category="LB|Network")
	FOnLBConnectionFailure OnConnectionFailure;

	UFUNCTION(BlueprintPure, Category="LB|Network")
	FText GetLastConnectionFailure() const { return LastConnectionFailure; }

	UFUNCTION(BlueprintCallable, Category="LB|Network")
	void ClearLastConnectionFailure() { LastConnectionFailure = FText::GetEmpty(); }

private:
	UPROPERTY(Transient)
	FText LastConnectionFailure;

	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;

	void HandleNetworkFailure(
		UWorld* World,
		UNetDriver* NetDriver,
		ENetworkFailure::Type FailureType,
		const FString& ErrorString);
	void HandleTravelFailure(
		UWorld* World,
		ETravelFailure::Type FailureType,
		const FString& ErrorString);
	bool IsFailureForThisInstance(const UWorld* World) const;
};
