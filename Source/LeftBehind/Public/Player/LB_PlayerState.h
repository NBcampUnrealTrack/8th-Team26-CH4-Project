//LBPlayerState.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "System/Raid/LBCharacterTypes.h"
#include "LB_PlayerState.generated.h"

class UAbilitySystemComponent;
class ULB_AbilitySystemComponent;
class ULB_AttributeSet;

// 역할 ID가 변경될 때 UI나 블루프린트가 반응할 수 있도록 알리는 델리게이트.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLBRoleChanged, ELBRoleType, NewRoleType);
// 사망 횟수가 바뀔 때 결과 집계/화면 갱신에 사용한다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLBDeathCountChanged, int32, NewDeathCount);
// 생존/사망 상태 변경을 UI, 리스폰 로직 등에 전달한다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLBDeadStateChanged, bool, bNewIsDead);

// 플레이어별 레이드 진행 정보와 GAS AbilitySystemComponent를 보관하는 PlayerState.
UCLASS()
class LEFTBEHIND_API ALB_PlayerState : public APlayerState, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ALB_PlayerState();

    // 역할, 레이드 통계, 사망 상태, MVP 여부를 클라이언트 복제 대상으로 등록한다.
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    // GAS가 이 PlayerState의 ASC를 찾을 때 사용하는 표준 인터페이스 구현.
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    // 프로젝트 전용 ASC를 그대로 돌려준다. 블루프린트에서 세부 기능을 쓸 때 사용한다.
    UFUNCTION(BlueprintPure, Category="LB|GAS")
    ULB_AbilitySystemComponent* GetLBAbilitySystemComponent() const;

    // PlayerState가 들고 있는 AttributeSet이다. Pawn이 바뀌어도 체력/마나 상태를 유지한다.
    UFUNCTION(BlueprintPure, Category="LB|GAS")
    ULB_AttributeSet* GetLBAttributeSet() const;

    // 블루프린트/UI에서 역할 변경 이벤트를 구독한다.
    UPROPERTY(BlueprintAssignable)
    FOnLBRoleChanged OnRoleChanged;

    // 블루프린트/UI에서 사망 횟수 변경 이벤트를 구독한다.
    UPROPERTY(BlueprintAssignable)
    FOnLBDeathCountChanged OnDeathCountChanged;

    // 블루프린트/UI에서 현재 사망 상태 변경 이벤트를 구독한다.
    UPROPERTY(BlueprintAssignable)
    FOnLBDeadStateChanged OnDeadStateChanged;

    // 레이드 시작/재시작 시 서버에서만 사망 정보, 누적 전투 통계, MVP 여부를 초기화한다.
    UFUNCTION(BlueprintCallable, Category="LB|PlayerState")
    void ResetRaidStats_ServerOnly();

    // 서버 권한으로 플레이어의 레이드 역할을 지정한다.
    UFUNCTION(BlueprintCallable, Category="LB|PlayerState")
    void SetRoleType_ServerOnly(ELBRoleType NewRoleType);

    // 서버 권한으로 선택한 캐릭터 ID를 지정한다. (호출 위치: 캐릭터 선택창에서 확정 시)
    UFUNCTION(BlueprintCallable, Category="LB|PlayerState")
    void SetCharacterID_ServerOnly(ELBCharacterID NewCharacterID);
    
    // 서버 권한으로 현재 사망 상태를 갱신한다.
    UFUNCTION(BlueprintCallable, Category="LB|PlayerState")
    void SetDead_ServerOnly(bool bNewDead);

    // 서버 권한으로 사망 횟수를 1 증가시킨다.
    UFUNCTION(BlueprintCallable, Category="LB|PlayerState")
    void AddDeathCount_ServerOnly();

    // 보스에게 피해를 줄 때마다 서버에서 누적한다. (호출 위치: 보스 TakeDamage)
    UFUNCTION(BlueprintCallable, Category="LB|Stats")
    void AddTotalDamageDealt_ServerOnly(float Amount);
    
    // 파티원을 치유할 때마다 서버에서 누적한다. (호출 위치: 힐 어빌리티 ApplyHeal 이후 / 오버힐 제외한 실제 적용된 회복량)
    UFUNCTION(BlueprintCallable, Category="LB|Stats")
    void AddTotalHealingDone_ServerOnly(float Amount);
    
    // 서버 권한으로 MVP 여부를 지정한다.
    UFUNCTION(BlueprintCallable, Category="LB|Stats")
    void SetMVP_ServerOnly(bool bNewMVP);
    
    // 현재 역할 타입을 읽는다.
    UFUNCTION(BlueprintPure, Category="LB|PlayerState")
    ELBRoleType GetRoleType() const { return RoleType; }
    
    // 현재 캐릭터 ID를 읽는다.
    UFUNCTION(BlueprintPure, Category="LB|PlayerState")
    ELBCharacterID GetCharacterID() const { return CharacterID; }
    
    // 현재 플레이어 이름을 반환한다.
    UFUNCTION(BlueprintPure, Category="LB|PlayerState")
    FText GetPlayerNameText() const;
    
    UFUNCTION(Server, BlueprintCallable, Reliable)
    void ServerRPCSetPlayerName(const FString& InName);

    // 누적 사망 횟수를 읽는다.
    UFUNCTION(BlueprintPure, Category="LB|PlayerState")
    int32 GetDeathCount() const { return DeathCount; }

    // 현재 사망 상태를 읽는다.
    UFUNCTION(BlueprintPure, Category="LB|PlayerState")
    bool IsDead() const { return bIsDead; }
    
    // 레이드 중 보스에게 입힌 총 피해량. 결과 화면 및 MVP 선정에 사용한다.
    UFUNCTION(BlueprintPure, Category="LB|Stats")
    float GetTotalDamageDealt() const { return TotalDamageDealt; }

    // 레이드 중 파티원에게 회복시킨 총 힐량. 힐러 MVP 선정에 사용한다.
    UFUNCTION(BlueprintPure, Category="LB|Stats")
    float GetTotalHealingDone() const { return TotalHealingDone; }

    // MVP 여부를 읽는다.
    UFUNCTION(BlueprintPure, Category="LB|Stats")
    bool GetIsMVP() const { return bIsMVP; }

protected:
    // PlayerState에 붙는 ASC. Pawn 교체/리스폰이 있어도 능력 상태를 유지하기 쉽다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS")
    TObjectPtr<ULB_AbilitySystemComponent> AbilitySystemComponent;

    // 플레이어 체력/마나 수치를 담는 GAS 데이터 묶음이다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS")
    TObjectPtr<ULB_AttributeSet> AttributeSet;

    // 레이드 역할 식별자. 클라이언트에는 OnRep_RoleType를 통해 변경 이벤트가 전달된다.
    UPROPERTY(ReplicatedUsing=OnRep_RoleType, BlueprintReadOnly, Category="LB|Raid")
    ELBRoleType RoleType = ELBRoleType::DPS;
    
    // 레이드 중 누적 사망 횟수. 결과 집계에서 전체 플레이어 사망 수 계산에 사용한다.
    UPROPERTY(ReplicatedUsing=OnRep_DeathCount, BlueprintReadOnly, Category="LB|Raid")
    int32 DeathCount = 0;

    // 현재 사망 여부. 모든 플레이어 사망 판정과 UI 상태 표시에 사용한다.
    UPROPERTY(ReplicatedUsing=OnRep_IsDead, BlueprintReadOnly, Category="LB|Raid")
    bool bIsDead = false;
    
    // 선택한 캐릭터 ID
    UPROPERTY(ReplicatedUsing=OnRep_CharacterID, BlueprintReadOnly, Category="LB|Raid")
    ELBCharacterID CharacterID = ELBCharacterID::None;
    
    // 보스에게 입힌 누적 총 피해량.
    UPROPERTY(Replicated, BlueprintReadOnly, Category="LB|Stats")
    float TotalDamageDealt = 0.f;
    
    // 파티원에게 회복시킨 누적 총 힐량.
    UPROPERTY(Replicated, BlueprintReadOnly, Category="LB|Stats")
    float TotalHealingDone = 0.f;
    
    // MVP인지 여부.
    UPROPERTY(Replicated, BlueprintReadOnly, Category="LB|Stats")
    bool bIsMVP = false;

    // RoleID가 복제되거나 서버에서 직접 갱신된 직후 역할 변경 델리게이트를 방송한다.
    UFUNCTION()
    void OnRep_RoleType();

    // DeathCount가 복제되거나 서버에서 직접 갱신된 직후 사망 횟수 변경 델리게이트를 방송한다.
    UFUNCTION()
    void OnRep_DeathCount();

    // bIsDead가 복제되거나 서버에서 직접 갱신된 직후 사망 상태 변경 델리게이트를 방송한다.
    UFUNCTION()
    void OnRep_IsDead();
    
    UFUNCTION()
    void OnRep_CharacterID();
};
