#pragma once

#include "CoreMinimal.h"
#include "System/Raid/LBCharacterTypes.h"

namespace LBRaidMVP
{
	// 기존 비즈니스 규칙: 역할 주 기여도 80%, 보조 기여도 20%를 합산한다.
	// 상수를 한곳에 고정해 서버 결과, 자동화 테스트, 향후 밸런싱 코드의 산식 불일치를 막는다.
	inline constexpr float DefaultPrimaryWeight = 0.8f;
	// 부동소수점 미세 오차가 플랫폼별 MVP를 바꾸지 않도록 동점 판정 허용 오차를 고정한다.
	inline constexpr float ScoreComparisonEpsilon = 1.e-4f;

	// PlayerState와 무관하게 MVP 산식을 검증할 수 있도록 계산에 필요한 값만 담는다.
	struct FScoringInput
	{
		ELBRoleType RoleType = ELBRoleType::DPS;
		float TotalDamageDealt = 0.f;
		float TotalHealingDone = 0.f;
		int32 DeathCount = 0;
		int32 PlayerId = INDEX_NONE;
	};

	// 한 레이드의 역할별/전체 최대값. 서로 단위가 다른 피해와 회복을 0~1로 정규화할 때 사용한다.
	struct FScoringContext
	{
		float MaxDPSDamage = 0.f;
		float MaxHealerHealing = 0.f;
		float MaxAllDamage = 0.f;
		float MaxAllHealing = 0.f;
	};

	struct FScoringResult
	{
		float Score = 0.f;
		float PrimaryNormalized = 0.f;
		float SecondaryNormalized = 0.f;
	};

	// 네트워크/게임플레이 경계에서 유입될 수 있는 NaN, Infinity, 음수 통계를 0으로 정규화한다.
	// 비정상 float가 최대값과 최종 Score에 전파되면 모든 후보 비교가 무효화되는 문제를 사전에 차단한다.
	inline float SanitizeStat(const float Value)
	{
		return FMath::IsFinite(Value) && Value > 0.f ? Value : 0.f;
	}

	inline int32 SanitizeDeathCount(const int32 Value)
	{
		return FMath::Max(0, Value);
	}

	// 분모가 없을 때는 기여도 0을 반환하고 정상 값은 0~1 범위로 제한한다.
	// 최대 float에서도 곱셈 없이 비율만 계산하므로 불필요한 overflow 가능성과 추가 메모리 할당이 없다.
	inline float SafeNormalizedValue(const float Value, const float Maximum)
	{
		const float SafeValue = SanitizeStat(Value);
		const float SafeMaximum = SanitizeStat(Maximum);
		return SafeMaximum > 0.f ? FMath::Clamp(SafeValue / SafeMaximum, 0.f, 1.f) : 0.f;
	}

	inline FScoringContext BuildScoringContext(const TConstArrayView<FScoringInput> Inputs)
	{
		FScoringContext Context;

		// TConstArrayView를 사용해 결과 배열을 복사하지 않고 한 번의 순회로 역할별/전체 최대값을 수집한다.
		for (const FScoringInput& Input : Inputs)
		{
			const float Damage = SanitizeStat(Input.TotalDamageDealt);
			const float Healing = SanitizeStat(Input.TotalHealingDone);

			Context.MaxAllDamage = FMath::Max(Context.MaxAllDamage, Damage);
			Context.MaxAllHealing = FMath::Max(Context.MaxAllHealing, Healing);

			switch (Input.RoleType)
			{
			case ELBRoleType::DPS:
				Context.MaxDPSDamage = FMath::Max(Context.MaxDPSDamage, Damage);
				break;

			case ELBRoleType::Healer:
				Context.MaxHealerHealing = FMath::Max(Context.MaxHealerHealing, Healing);
				break;

			default:
				break;
			}
		}

		return Context;
	}

	inline FScoringResult CalculateScore(
		const FScoringInput& Input,
		const FScoringContext& Context,
		const float PrimaryWeight = DefaultPrimaryWeight)
	{
		FScoringResult Result;
		// 잘못된 설정값은 기존 80/20 기본 규칙으로 복구하고, 유한한 범위 밖 값은 0~1로 제한한다.
		const float SafePrimaryWeight = FMath::Clamp(
			FMath::IsFinite(PrimaryWeight) ? PrimaryWeight : DefaultPrimaryWeight,
			0.f,
			1.f);
		const float SecondaryWeight = 1.f - SafePrimaryWeight;

		switch (Input.RoleType)
		{
		case ELBRoleType::DPS:
			Result.PrimaryNormalized = SafeNormalizedValue(Input.TotalDamageDealt, Context.MaxDPSDamage);
			Result.SecondaryNormalized = SafeNormalizedValue(Input.TotalHealingDone, Context.MaxAllHealing);
			break;

		case ELBRoleType::Healer:
			Result.PrimaryNormalized = SafeNormalizedValue(Input.TotalHealingDone, Context.MaxHealerHealing);
			Result.SecondaryNormalized = SafeNormalizedValue(Input.TotalDamageDealt, Context.MaxAllDamage);
			break;

		default:
			return Result;
		}

		Result.Score =
			SafePrimaryWeight * Result.PrimaryNormalized
			+ SecondaryWeight * Result.SecondaryNormalized;
		return Result;
	}

	// 완전 동점 순서 계약: Score > 적은 사망 > 주 기여 > 보조 기여 > 유효하고 낮은 PlayerId > 원본 배열 순서.
	// 서버 컨테이너 순서나 부동소수점 오차에 따라 MVP가 달라지지 않도록 모든 tie-break를 결정적으로 유지한다.
	inline bool IsBetterCandidate(
		const FScoringInput& CandidateInput,
		const FScoringResult& CandidateResult,
		const int32 CandidateStableIndex,
		const FScoringInput& BestInput,
		const FScoringResult& BestResult,
		const int32 BestStableIndex,
		const float Epsilon = ScoreComparisonEpsilon)
	{
		const float SafeEpsilon = FMath::IsFinite(Epsilon)
			? FMath::Max(0.f, Epsilon)
			: ScoreComparisonEpsilon;

		if (CandidateResult.Score > BestResult.Score + SafeEpsilon)
		{
			return true;
		}
		if (BestResult.Score > CandidateResult.Score + SafeEpsilon)
		{
			return false;
		}

		const int32 CandidateDeaths = SanitizeDeathCount(CandidateInput.DeathCount);
		const int32 BestDeaths = SanitizeDeathCount(BestInput.DeathCount);
		if (CandidateDeaths != BestDeaths)
		{
			return CandidateDeaths < BestDeaths;
		}

		if (CandidateResult.PrimaryNormalized > BestResult.PrimaryNormalized + SafeEpsilon)
		{
			return true;
		}
		if (BestResult.PrimaryNormalized > CandidateResult.PrimaryNormalized + SafeEpsilon)
		{
			return false;
		}

		if (CandidateResult.SecondaryNormalized > BestResult.SecondaryNormalized + SafeEpsilon)
		{
			return true;
		}
		if (BestResult.SecondaryNormalized > CandidateResult.SecondaryNormalized + SafeEpsilon)
		{
			return false;
		}

		const bool bCandidateHasValidPlayerId = CandidateInput.PlayerId >= 0;
		const bool bBestHasValidPlayerId = BestInput.PlayerId >= 0;
		if (bCandidateHasValidPlayerId != bBestHasValidPlayerId)
		{
			return bCandidateHasValidPlayerId;
		}
		if (bCandidateHasValidPlayerId && CandidateInput.PlayerId != BestInput.PlayerId)
		{
			return CandidateInput.PlayerId < BestInput.PlayerId;
		}

		return CandidateStableIndex < BestStableIndex;
	}

	inline int32 SelectMVPIndex(
		const TConstArrayView<FScoringInput> Inputs,
		const float PrimaryWeight = DefaultPrimaryWeight,
		const float Epsilon = ScoreComparisonEpsilon)
	{
		if (Inputs.IsEmpty())
		{
			return INDEX_NONE;
		}

		const float SafeEpsilon = FMath::IsFinite(Epsilon)
			? FMath::Max(0.f, Epsilon)
			: ScoreComparisonEpsilon;
		const FScoringContext Context = BuildScoringContext(Inputs);

		int32 BestIndex = INDEX_NONE;
		FScoringResult BestResult;

		// 별도 정렬/후보 배열 없이 O(N) 한 번으로 선택해 레이드 종료 순간의 할당과 복사 비용을 최소화한다.
		for (int32 Index = 0; Index < Inputs.Num(); ++Index)
		{
			const FScoringResult CandidateResult = CalculateScore(Inputs[Index], Context, PrimaryWeight);
			if (BestIndex == INDEX_NONE
				|| IsBetterCandidate(
					Inputs[Index],
					CandidateResult,
					Index,
					Inputs[BestIndex],
					BestResult,
					BestIndex,
					SafeEpsilon))
			{
				BestIndex = Index;
				BestResult = CandidateResult;
			}
		}

		// 아무도 유효한 기여를 하지 않았다면 동점 규칙으로 임의 MVP를 만들지 않는다.
		return BestIndex != INDEX_NONE && BestResult.Score > SafeEpsilon ? BestIndex : INDEX_NONE;
	}
}
