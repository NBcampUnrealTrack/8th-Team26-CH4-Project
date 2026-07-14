#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "LB_OnlineSessionSubsystem.generated.h"

UENUM(BlueprintType)
enum class ELBOnlineState : uint8
{
	SignedOut,
	SigningIn,
	Ready,
	Searching,
	Creating,
	Joining,
	InRoom,
	Leaving,
	Traveling,
	Error
};

UENUM(BlueprintType)
enum class ELBRoomPhase : uint8
{
	Waiting,
	CharacterSelect,
	InRaid
};

USTRUCT(BlueprintType)
struct LEFTBEHIND_API FLBRoomSummary
{
	GENERATED_BODY()

	/** Opaque online room id. UI code should never parse this value. */
	UPROPERTY(BlueprintReadOnly, Category="LB|Online")
	FString RoomId;

	UPROPERTY(BlueprintReadOnly, Category="LB|Online")
	FString HostDisplayName;

	UPROPERTY(BlueprintReadOnly, Category="LB|Online")
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category="LB|Online")
	int32 MaxPlayers = 4;

	UPROPERTY(BlueprintReadOnly, Category="LB|Online")
	bool bCanJoin = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnLBOnlineStateChanged,
	ELBOnlineState, NewState,
	const FText&, StatusMessage);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnLBRoomsChanged,
	const TArray<FLBRoomSummary>&, Rooms);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnLBRoomPhaseUpdateComplete,
	bool, bWasSuccessful,
	ELBRoomPhase, Phase,
	const FText&, ErrorMessage);

class FLBOnlineSessionRuntime;

/**
 * Owns every OSSv1 identity/session operation for the local game instance.
 *
 * Widgets only call this API and subscribe to its events. They must not retain
 * FOnlineSessionSearchResult values or bind directly to OnlineSubsystem delegates.
 */
UCLASS()
class LEFTBEHIND_API ULB_OnlineSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Reuses Online PIE login, consumes AUTH_* credentials, or opens Account Portal. */
	UFUNCTION(BlueprintCallable, Category="LB|Online")
	bool SignIn();

	/** Creates the fixed-policy four-player EOS lobby. */
	UFUNCTION(BlueprintCallable, Category="LB|Online")
	bool CreateRoom();

	/** Replaces the current room snapshot with a fresh online room search. */
	UFUNCTION(BlueprintCallable, Category="LB|Online")
	bool RefreshRooms();

	/** Joins only a currently published room id; stale selections are rejected. */
	UFUNCTION(BlueprintCallable, Category="LB|Online")
	bool JoinRoom(const FString& RoomId);

	/** Leaves/destroys the current room and returns to L_MainMenu. */
	UFUNCTION(BlueprintCallable, Category="LB|Online")
	bool LeaveRoom();

	/** Opens the EOS friends overlay, from which a presence invite can be sent. */
	UFUNCTION(BlueprintCallable, Category="LB|Online")
	bool OpenSocialOverlay();

	/** Returns whether this process can open the EOS friends overlay and why not. */
	bool CanOpenSocialOverlay(FText* OutUnavailableReason = nullptr) const;

	/**
	 * Host-only async update performed before ServerTravel to the raid.
	 * Wait for OnRoomPhaseUpdateComplete(success, InRaid, ...) before traveling.
	 */

	UFUNCTION(BlueprintCallable, Category="LB|Online")
	bool StartCharacterSelect();

	UFUNCTION(BlueprintCallable, Category="LB|Online")
	bool StartRaid();
	
	/** Reopens the host lobby after returning from the raid. */
	UFUNCTION(BlueprintCallable, Category="LB|Online")
	bool ReopenRoomAfterRaid();

	UFUNCTION(BlueprintPure, Category="LB|Online")
	bool IsInRoom() const;

	UFUNCTION(BlueprintPure, Category="LB|Online")
	bool IsRoomHost() const;

	UFUNCTION(BlueprintPure, Category="LB|Online")
	TArray<FLBRoomSummary> GetRooms() const { return Rooms; }

	UFUNCTION(BlueprintPure, Category="LB|Online")
	ELBOnlineState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category="LB|Online")
	FText GetLastError() const { return LastError; }

	UPROPERTY(BlueprintAssignable, Category="LB|Online")
	FOnLBOnlineStateChanged OnStateChanged;

	UPROPERTY(BlueprintAssignable, Category="LB|Online")
	FOnLBRoomsChanged OnRoomsChanged;

	UPROPERTY(BlueprintAssignable, Category="LB|Online")
	FOnLBRoomPhaseUpdateComplete OnRoomPhaseUpdateComplete;

private:
	friend class FLBOnlineSessionRuntime;

	UPROPERTY(Transient)
	ELBOnlineState State = ELBOnlineState::SignedOut;

	UPROPERTY(Transient)
	TArray<FLBRoomSummary> Rooms;

	UPROPERTY(Transient)
	FText LastError;

	TSharedPtr<FLBOnlineSessionRuntime> Runtime;

	void SetState(ELBOnlineState NewState, const FText& StatusMessage = FText::GetEmpty());
	void SetRooms(TArray<FLBRoomSummary>&& NewRooms);
};

