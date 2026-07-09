#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "LBCharacterTypes.generated.h"

UENUM(BlueprintType)
enum class ELBRoleType : uint8
{
	DPS     UMETA(DisplayName="딜러"),
	Healer  UMETA(DisplayName="힐러")
};


// [필독] 캐릭터 추가 시 아래 두 곳 모두 수정해야 함
	// 1. ELBCharacterID -- Enum 값 추가
	// 2. DT_CharacterData -- Enum 이름과 동일한 Row Name으로 행 추가 (대소문자 정확히 일치해야 합니다)
UENUM(BlueprintType)
enum class ELBCharacterID : uint8
{
	None        UMETA(DisplayName="없음"),
	Boris       UMETA(DisplayName="보리스"),
	Dekker      UMETA(DisplayName="데커"),
	Grux        UMETA(DisplayName="그럭스"),
	IggyScorch  UMETA(DisplayName="이기스코치"),
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

	// 결과 카드, 대기실 등 UI에서 사용할 역할 아이콘
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> RoleIcon;

	// 역할 대표 컬러
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor RoleColor = FLinearColor::White;

	// 역할 타입 -- MVP 선정 및 통계 분류에 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ELBRoleType RoleType  = ELBRoleType::DPS;
};


// 캐릭터 데이터 테이블 행 구조체 -- DT_CharacterData의 Row Type으로 사용
USTRUCT(BlueprintType)
struct FLBCharacterData : public FTableRowBase
{
	GENERATED_BODY()

	// 화면 표시용 캐릭터 이름
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	// 캐릭터 선택창 카드에 표시할 초상화 이미지
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> CardPortraitImage;

	// 결과 화면 카드에 표시할 전신 이미지
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> HUDPortraitImage;

	// 캐릭터 선택창에서 역할별 분류에 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ELBRoleType RoleType  = ELBRoleType::DPS;

	// 캐릭터 선택창에서 3D 프리뷰로 보여줄 스켈레탈 메시
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<USkeletalMesh> PreviewMesh;

	// true면 캐릭터 선택창에서 선택 불가 상태로 표시
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bLocked = false;
};