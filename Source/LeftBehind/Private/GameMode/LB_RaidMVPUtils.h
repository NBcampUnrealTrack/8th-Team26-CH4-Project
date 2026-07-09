#pragma once

#include "CoreMinimal.h"
#include "System/Raid/LBCharacterTypes.h"

namespace LBRaidMVP
{
	inline constexpr float DefaultPrimaryWeight = 0.8f;
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

	inline float SanitizeStat(const float Value)
	{
		return FMath::IsFinite(Value) && Value > 0.f ? Value : 0.f;
	}

	inline int32 SanitizeDeathCount(const int32 Value)
	{
		return FMath::Max(0, Value);
	}

	inline float SafeNormalizedValue(const float Value, const float Maximum)
	{
		const float SafeValue = SanitizeStat(Value);
		const float SafeMaximum = SanitizeStat(Maximum);
		return SafeMaximum > 0.f ? FMath::Clamp(SafeValue / SafeMaximum, 0.f, 1.f) : 0.f;
	}

	inline FScoringContext BuildScoringContext(const TConstArrayView<FScoringInput> Inputs)
	{
		FScoringContext Context;

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
