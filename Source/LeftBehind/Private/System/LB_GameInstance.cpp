#include "System/LB_GameInstance.h"

#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY_STATIC(LogLBGameInstance, Log, All);

void ULB_GameInstance::Init()
{
	Super::Init();

	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(
			this,
			&ThisClass::HandleNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(
			this,
			&ThisClass::HandleTravelFailure);
	}
}

void ULB_GameInstance::Shutdown()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}

	NetworkFailureHandle.Reset();
	TravelFailureHandle.Reset();
	Super::Shutdown();
}

void ULB_GameInstance::HandleNetworkFailure(
	UWorld* World,
	UNetDriver* NetDriver,
	ENetworkFailure::Type FailureType,
	const FString& ErrorString)
{
	(void)NetDriver;
	if (!IsFailureForThisInstance(World))
	{
		return;
	}

	LastConnectionFailure = FText::Format(
		NSLOCTEXT("LeftBehind", "NetworkFailure", "Network failure ({0}): {1}"),
		FText::FromString(ENetworkFailure::ToString(FailureType)),
		FText::FromString(ErrorString));
	UE_LOG(LogLBGameInstance, Error, TEXT("%s"), *LastConnectionFailure.ToString());
	OnConnectionFailure.Broadcast(LastConnectionFailure);
	// UEngine가 클라이언트 실패에 대해 CallHandleDisconnectForFailure를 수행하고 기본 맵으로 복귀시킨다.
}

void ULB_GameInstance::HandleTravelFailure(
	UWorld* World,
	ETravelFailure::Type FailureType,
	const FString& ErrorString)
{
	if (!IsFailureForThisInstance(World))
	{
		return;
	}

	LastConnectionFailure = FText::Format(
		NSLOCTEXT("LeftBehind", "TravelFailure", "Travel failure ({0}): {1}"),
		FText::FromString(ETravelFailure::ToString(FailureType)),
		FText::FromString(ErrorString));
	UE_LOG(LogLBGameInstance, Error, TEXT("%s"), *LastConnectionFailure.ToString());
	OnConnectionFailure.Broadcast(LastConnectionFailure);
}

bool ULB_GameInstance::IsFailureForThisInstance(const UWorld* World) const
{
	return !World || World->GetGameInstance() == this;
}
