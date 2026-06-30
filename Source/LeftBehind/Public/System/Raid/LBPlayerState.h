//LBPlayerState.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "LBPlayerState.generated.h"

class UAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLBRoleChanged, FName, NewRoleID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLBDeathCountChanged, int32, NewDeathCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLBDeadStateChanged, bool, bNewIsDead);

UCLASS()
class LEFTBEHIND_API ALBPlayerState : public APlayerState, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ALBPlayerState();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    UPROPERTY(BlueprintAssignable)
    FOnLBRoleChanged OnRoleChanged;

    UPROPERTY(BlueprintAssignable)
    FOnLBDeathCountChanged OnDeathCountChanged;

    UPROPERTY(BlueprintAssignable)
    FOnLBDeadStateChanged OnDeadStateChanged;

    UFUNCTION(BlueprintCallable, Category="LB|PlayerState")
    void ResetRaidStats_ServerOnly();

    UFUNCTION(BlueprintCallable, Category="LB|PlayerState")
    void SetRoleID_ServerOnly(FName NewRoleID);

    UFUNCTION(BlueprintCallable, Category="LB|PlayerState")
    void SetDead_ServerOnly(bool bNewDead);

    UFUNCTION(BlueprintCallable, Category="LB|PlayerState")
    void AddDeathCount_ServerOnly();

    UFUNCTION(BlueprintPure, Category="LB|PlayerState")
    FName GetRoleID() const { return RoleID; }

    UFUNCTION(BlueprintPure, Category="LB|PlayerState")
    int32 GetDeathCount() const { return DeathCount; }

    UFUNCTION(BlueprintPure, Category="LB|PlayerState")
    bool IsDead() const { return bIsDead; }

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS")
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY(ReplicatedUsing=OnRep_RoleID, BlueprintReadOnly, Category="LB|Raid")
    FName RoleID = "Role_DPS";

    UPROPERTY(ReplicatedUsing=OnRep_DeathCount, BlueprintReadOnly, Category="LB|Raid")
    int32 DeathCount = 0;

    UPROPERTY(ReplicatedUsing=OnRep_IsDead, BlueprintReadOnly, Category="LB|Raid")
    bool bIsDead = false;

    UFUNCTION()
    void OnRep_RoleID();

    UFUNCTION()
    void OnRep_DeathCount();

    UFUNCTION()
    void OnRep_IsDead();
};