//LBPlayerState.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "LBPlayerState.generated.h"

class UAbilitySystemComponent;

// 역할 ID가 변경될 때 UI나 블루프린트가 반응할 수 있도록 알리는 델리게이트.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLBRoleChanged, FName, NewRoleID);
// 사망 횟수가 바뀔 때 결과 집계/화면 갱신에 사용한다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLBDeathCountChanged, int32, NewDeathCount);
// 생존/사망 상태 변경을 UI, 리스폰 로직 등에 전달한다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLBDeadStateChanged, bool, bNewIsDead);

// 플레이어별 레이드 진행 정보와 GAS AbilitySystemComponent를 보관하는 PlayerState.
UCLASS()
class LEFTBEHIND_API ALBPlayerState : public APlayerState, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ALBPlayerState();

    // RoleID, DeathCount, bIsDead를 클라이언트에 복제 대상으로 등록한다.
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    // GAS가 이 PlayerState의 ASC를 찾을 때 사용하는 표준 인터페이스 구현.
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    // 블루프린트/UI에서 역할 변경 이벤트를 구독한다.
    UPROPERTY(BlueprintAssignable)
    FOnLBRoleChanged OnRoleChanged;

    // 블루프린트/UI에서 사망 횟수 변경 이벤트를 구독한다.
    UPROPERTY(BlueprintAssignable)
    FOnLBDeathCountChanged OnDeathCountChanged;

    // 블루프린트/UI에서 현재 사망 상태 변경 이벤트를 구독한다.
    UPROPERTY(BlueprintAssignable)
    FOnLBDeadStateChanged OnDeadStateChanged;

    // 레이드 시작/재시작 시 서버에서만 누적 사망 정보와 사망 상태를 초기화한다.
    UFUNCTION(BlueprintCallable, Category="LB|PlayerState")
    void ResetRaidStats_ServerOnly();

    // 서버 권한으로 플레이어의 레이드 역할을 지정한다.
    UFUNCTION(BlueprintCallable, Category="LB|PlayerState")
    void SetRoleID_ServerOnly(FName NewRoleID);

    // 서버 권한으로 현재 사망 상태를 갱신한다.
    UFUNCTION(BlueprintCallable, Category="LB|PlayerState")
    void SetDead_ServerOnly(bool bNewDead);

    // 서버 권한으로 사망 횟수를 1 증가시킨다.
    UFUNCTION(BlueprintCallable, Category="LB|PlayerState")
    void AddDeathCount_ServerOnly();

    // 현재 역할 ID를 읽는다.
    UFUNCTION(BlueprintPure, Category="LB|PlayerState")
    FName GetRoleID() const { return RoleID; }

    // 누적 사망 횟수를 읽는다.
    UFUNCTION(BlueprintPure, Category="LB|PlayerState")
    int32 GetDeathCount() const { return DeathCount; }

    // 현재 사망 상태를 읽는다.
    UFUNCTION(BlueprintPure, Category="LB|PlayerState")
    bool IsDead() const { return bIsDead; }

protected:
    // PlayerState에 붙는 ASC. Pawn 교체/리스폰이 있어도 능력 상태를 유지하기 쉽다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS")
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    // 레이드 역할 식별자. 클라이언트에는 OnRep_RoleID를 통해 변경 이벤트가 전달된다.
    UPROPERTY(ReplicatedUsing=OnRep_RoleID, BlueprintReadOnly, Category="LB|Raid")
    FName RoleID = "Role_DPS";

    // 레이드 중 누적 사망 횟수. 결과 집계에서 전체 플레이어 사망 수 계산에 사용한다.
    UPROPERTY(ReplicatedUsing=OnRep_DeathCount, BlueprintReadOnly, Category="LB|Raid")
    int32 DeathCount = 0;

    // 현재 사망 여부. 모든 플레이어 사망 판정과 UI 상태 표시에 사용한다.
    UPROPERTY(ReplicatedUsing=OnRep_IsDead, BlueprintReadOnly, Category="LB|Raid")
    bool bIsDead = false;

    // RoleID가 복제되거나 서버에서 직접 갱신된 직후 역할 변경 델리게이트를 방송한다.
    UFUNCTION()
    void OnRep_RoleID();

    // DeathCount가 복제되거나 서버에서 직접 갱신된 직후 사망 횟수 변경 델리게이트를 방송한다.
    UFUNCTION()
    void OnRep_DeathCount();

    // bIsDead가 복제되거나 서버에서 직접 갱신된 직후 사망 상태 변경 델리게이트를 방송한다.
    UFUNCTION()
    void OnRep_IsDead();
};
