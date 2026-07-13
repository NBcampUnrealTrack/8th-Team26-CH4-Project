#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstance.h"
#include "LBCharacterTypes.generated.h"

class USkeletalMesh;
class UTexture2D;

// 직렬화된 DataTable/Blueprint 값의 호환성을 보장하기 위해 enum ordinal을 명시적으로 고정한다.
UENUM(BlueprintType)
enum class ELBRoleType : uint8
{
	DPS = 0     UMETA(DisplayName="딜러"),
	Healer = 1  UMETA(DisplayName="힐러")
};

// [필독] 캐릭터 추가 시 아래 두 곳 모두 수정해야 함
// 1. ELBCharacterID -- 기존 숫자는 변경하지 말고 마지막에 새 Enum 값 추가
// 2. DT_CharacterData -- Enum 이름과 동일한 Row Name으로 행 추가 (대소문자 정확히 일치해야 합니다)
// ordinal 변경은 저장된 Blueprint/DataTable을 조용히 다른 캐릭터로 해석하게 하므로 값을 명시한다.
UENUM(BlueprintType)
enum class ELBCharacterID : uint8
{
	None = 0        UMETA(DisplayName="없음"),
	Boris = 1       UMETA(DisplayName="보리스"),
	Dekker = 2      UMETA(DisplayName="데커"),
	Grux = 3        UMETA(DisplayName="그럭스"),
	IggyScorch = 4  UMETA(DisplayName="이기스코치"),
};

// 역할 데이터 테이블 행 구조체 -- DT_RoleData의 Row Type으로 사용
USTRUCT(BlueprintType)
struct FLBRoleData : public FTableRowBase
{
	GENERATED_BODY()

	// 역할 이름
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText RoleName;

	// 라틴어 부제
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText RoleSubtitle;

	// 결과 카드, 대기실 등 UI에서 사용할 역할 아이콘.
	// Soft reference는 UI가 실제로 필요할 때만 텍스처를 로드해 초기 메모리와 로딩 비용을 줄인다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> RoleIcon;

	// 역할 대표 컬러
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor RoleColor = FLinearColor::White;

	// 역할 타입 -- MVP 선정 및 통계 분류에 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ELBRoleType RoleType = ELBRoleType::DPS;
};


// 캐릭터 데이터 테이블 행 구조체 -- DT_CharacterData의 Row Type으로 사용
USTRUCT(BlueprintType)
struct FLBCharacterData : public FTableRowBase
{
	GENERATED_BODY()

	// 화면 표시용 캐릭터 이름
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	// 캐릭터 선택창 카드에 표시할 초상화 이미지.
	// 선택 화면 진입 전까지 고해상도 이미지를 상주시킬 필요가 없어 Soft reference를 유지한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UTexture2D> CardPortraitImage;

	// 결과 화면 카드에 표시할 전신 이미지
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UTexture2D> HUDPortraitImage;

	// 캐릭터 선택창에서 역할별 분류에 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ELBRoleType RoleType = ELBRoleType::DPS;

	// 캐릭터 선택창에서 3D 프리뷰로 보여줄 스켈레탈 메시
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<USkeletalMesh> PreviewMesh;

	// true면 캐릭터 선택창에서 선택 불가 상태로 표시
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bLocked = false;
	
	// 캐릭터 상세 화면에 표시할 스탯
	// 사냥 등급 (예: "입문", "보통", "숙련", "전문")
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText HuntingGrade;

	// 공격력 수치
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 AttackPower = 0;

	// 방어력 수치
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Defense = 0;

	// 치명타 확률 (0~100)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 CriticalRate = 0;

	// 이동속도 (예: "느림", "보통", "빠름")
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText MoveSpeed;
	
	// 캐릭터 3D 프리뷰를 UI에 표시하기 위한 렌더 타깃
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UTextureRenderTarget2D> PreviewRenderTarget;

	// 렌더 타깃을 UI Image에 표시하기 위한 머티리얼 인스턴스
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UMaterialInstance> PreviewMaterialInstance;
};
