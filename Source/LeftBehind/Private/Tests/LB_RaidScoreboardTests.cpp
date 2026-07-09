#include "GameMode/LB_RaidMVPUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GameState/LB_RaidGameState.h"
#include "Misc/AutomationTest.h"
#include "Player/LB_PlayerState.h"
#include "UObject/UnrealType.h"

#include <limits>

namespace
{
	LBRaidMVP::FScoringInput MakeScoringInput(
		const ELBRoleType RoleType,
		const float Damage,
		const float Healing,
		const int32 DeathCount = 0,
		const int32 PlayerId = INDEX_NONE)
	{
		LBRaidMVP::FScoringInput Input;
		Input.RoleType = RoleType;
		Input.TotalDamageDealt = Damage;
		Input.TotalHealingDone = Healing;
		Input.DeathCount = DeathCount;
		Input.PlayerId = PlayerId;
		return Input;
	}

	bool IsNearlyEqual(const float A, const float B)
	{
		return FMath::IsNearlyEqual(A, B, KINDA_SMALL_NUMBER);
	}

	template <typename TObjectType>
	bool IsLifetimeReplicated(const TObjectType* DefaultObject, const FProperty* Property)
	{
		if (!DefaultObject || !Property)
		{
			return false;
		}

		TArray<FLifetimeProperty> LifetimeProperties;
		DefaultObject->GetLifetimeReplicatedProps(LifetimeProperties);
		return LifetimeProperties.ContainsByPredicate(
			[Property](const FLifetimeProperty& LifetimeProperty)
			{
				return LifetimeProperty.RepIndex == Property->RepIndex;
			});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBRaidMVPFormulaTest,
	"LeftBehind.Raid.Scoreboard.MVP.Formula",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBRaidMVPFormulaTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const TArray<LBRaidMVP::FScoringInput> Inputs = {
		MakeScoringInput(ELBRoleType::DPS, 50.f, 25.f),
		MakeScoringInput(ELBRoleType::Healer, 25.f, 100.f)
	};

	const LBRaidMVP::FScoringContext Context = LBRaidMVP::BuildScoringContext(Inputs);
	const LBRaidMVP::FScoringResult DPSResult = LBRaidMVP::CalculateScore(Inputs[0], Context, 0.8f);
	const LBRaidMVP::FScoringResult HealerResult = LBRaidMVP::CalculateScore(Inputs[1], Context, 0.8f);

	TestTrue(TEXT("DPS primary contribution is normalized by the maximum DPS damage"), IsNearlyEqual(DPSResult.PrimaryNormalized, 1.f));
	TestTrue(TEXT("DPS secondary contribution uses all-player healing"), IsNearlyEqual(DPSResult.SecondaryNormalized, 0.25f));
	TestTrue(TEXT("DPS score applies the 80/20 weights"), IsNearlyEqual(DPSResult.Score, 0.85f));
	TestTrue(TEXT("Healer primary contribution is normalized by the maximum healer healing"), IsNearlyEqual(HealerResult.PrimaryNormalized, 1.f));
	TestTrue(TEXT("Healer secondary contribution uses all-player damage"), IsNearlyEqual(HealerResult.SecondaryNormalized, 0.5f));
	TestTrue(TEXT("Healer score applies the 80/20 weights"), IsNearlyEqual(HealerResult.Score, 0.9f));

	const LBRaidMVP::FScoringResult PrimaryOnly = LBRaidMVP::CalculateScore(Inputs[0], Context, 2.f);
	const LBRaidMVP::FScoringResult SecondaryOnly = LBRaidMVP::CalculateScore(Inputs[0], Context, -1.f);
	const LBRaidMVP::FScoringResult DefaultedWeight = LBRaidMVP::CalculateScore(
		Inputs[0],
		Context,
		std::numeric_limits<float>::quiet_NaN());

	TestTrue(TEXT("Primary weight is clamped to one"), IsNearlyEqual(PrimaryOnly.Score, 1.f));
	TestTrue(TEXT("Primary weight is clamped to zero"), IsNearlyEqual(SecondaryOnly.Score, 0.25f));
	TestTrue(TEXT("A non-finite primary weight uses the default"), IsNearlyEqual(DefaultedWeight.Score, DPSResult.Score));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBRaidMVPRoleFairnessTest,
	"LeftBehind.Raid.Scoreboard.MVP.RoleFairness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBRaidMVPRoleFairnessTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const TArray<LBRaidMVP::FScoringInput> Inputs = {
		MakeScoringInput(ELBRoleType::DPS, 100.f, 0.f, 0, 1),
		MakeScoringInput(ELBRoleType::Healer, 30.f, 100.f, 0, 2)
	};

	TestEqual(
		TEXT("A healer can beat the top raw-damage player after role normalization"),
		LBRaidMVP::SelectMVPIndex(Inputs),
		1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBRaidMVPZeroContributionTest,
	"LeftBehind.Raid.Scoreboard.MVP.ZeroContribution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBRaidMVPZeroContributionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const TArray<LBRaidMVP::FScoringInput> EmptyInputs;
	const TArray<LBRaidMVP::FScoringInput> AllZeroInputs = {
		MakeScoringInput(ELBRoleType::DPS, 0.f, 0.f, 0, 1),
		MakeScoringInput(ELBRoleType::Healer, 0.f, 0.f, 0, 2)
	};
	const TArray<LBRaidMVP::FScoringInput> NoHealingInputs = {
		MakeScoringInput(ELBRoleType::DPS, 100.f, 0.f, 0, 1),
		MakeScoringInput(ELBRoleType::Healer, 25.f, 0.f, 0, 2)
	};

	TestEqual(TEXT("An empty raid has no MVP"), LBRaidMVP::SelectMVPIndex(EmptyInputs), INDEX_NONE);
	TestEqual(TEXT("All-zero contribution does not select an arbitrary MVP"), LBRaidMVP::SelectMVPIndex(AllZeroInputs), INDEX_NONE);

	const LBRaidMVP::FScoringContext Context = LBRaidMVP::BuildScoringContext(NoHealingInputs);
	const LBRaidMVP::FScoringResult DPSResult = LBRaidMVP::CalculateScore(NoHealingInputs[0], Context);
	TestTrue(TEXT("A zero healing denominator produces a finite score"), FMath::IsFinite(DPSResult.Score));
	TestTrue(TEXT("A zero healing denominator contributes zero secondary score"), IsNearlyEqual(DPSResult.SecondaryNormalized, 0.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBRaidMVPInvalidStatsTest,
	"LeftBehind.Raid.Scoreboard.MVP.InvalidStats",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBRaidMVPInvalidStatsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const float NaN = std::numeric_limits<float>::quiet_NaN();
	const float Infinity = std::numeric_limits<float>::infinity();
	const TArray<LBRaidMVP::FScoringInput> Inputs = {
		MakeScoringInput(ELBRoleType::DPS, NaN, -20.f, -5, 1),
		MakeScoringInput(ELBRoleType::Healer, Infinity, 50.f, 0, 2)
	};

	const LBRaidMVP::FScoringContext Context = LBRaidMVP::BuildScoringContext(Inputs);
	const LBRaidMVP::FScoringResult InvalidDPSResult = LBRaidMVP::CalculateScore(Inputs[0], Context);

	TestTrue(TEXT("NaN is sanitized to zero"), IsNearlyEqual(LBRaidMVP::SanitizeStat(NaN), 0.f));
	TestTrue(TEXT("Infinity is sanitized to zero"), IsNearlyEqual(LBRaidMVP::SanitizeStat(Infinity), 0.f));
	TestTrue(TEXT("Negative contribution is sanitized to zero"), IsNearlyEqual(LBRaidMVP::SanitizeStat(-1.f), 0.f));
	TestTrue(TEXT("Invalid input cannot create a non-finite score"), FMath::IsFinite(InvalidDPSResult.Score));
	TestEqual(TEXT("The player with valid healing wins"), LBRaidMVP::SelectMVPIndex(Inputs), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBRaidMVPTieBreakTest,
	"LeftBehind.Raid.Scoreboard.MVP.TieBreaks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBRaidMVPTieBreakTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TArray<LBRaidMVP::FScoringInput> Inputs = {
		MakeScoringInput(ELBRoleType::DPS, 100.f, 0.f, 2, 1),
		MakeScoringInput(ELBRoleType::DPS, 100.f, 0.f, 1, 2)
	};
	TestEqual(TEXT("Fewer deaths break an equal-score tie"), LBRaidMVP::SelectMVPIndex(Inputs), 1);

	Inputs[0].DeathCount = 0;
	Inputs[1].DeathCount = 0;
	Inputs[0].PlayerId = 9;
	Inputs[1].PlayerId = 2;
	TestEqual(TEXT("The lower valid PlayerId breaks a remaining tie"), LBRaidMVP::SelectMVPIndex(Inputs), 1);

	Inputs[0].PlayerId = INDEX_NONE;
	Inputs[1].PlayerId = 2;
	TestEqual(TEXT("A valid PlayerId sorts before a negative PlayerId"), LBRaidMVP::SelectMVPIndex(Inputs), 1);

	Inputs[0].PlayerId = INDEX_NONE;
	Inputs[1].PlayerId = INDEX_NONE;
	TestEqual(TEXT("Original array order is the final stable tie-break"), LBRaidMVP::SelectMVPIndex(Inputs), 0);

	const LBRaidMVP::FScoringInput TieInput = MakeScoringInput(ELBRoleType::DPS, 1.f, 1.f, 0, 1);
	LBRaidMVP::FScoringResult HigherPrimary;
	HigherPrimary.Score = 0.8f;
	HigherPrimary.PrimaryNormalized = 1.f;
	HigherPrimary.SecondaryNormalized = 0.f;
	LBRaidMVP::FScoringResult HigherSecondary;
	HigherSecondary.Score = 0.8f;
	HigherSecondary.PrimaryNormalized = 0.75f;
	HigherSecondary.SecondaryNormalized = 1.f;

	TestTrue(
		TEXT("Primary normalized contribution is checked before secondary contribution"),
		LBRaidMVP::IsBetterCandidate(TieInput, HigherPrimary, 1, TieInput, HigherSecondary, 0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBRaidScoreboardReplicationRegistrationTest,
	"LeftBehind.Raid.Scoreboard.ReplicationRegistration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBRaidScoreboardReplicationRegistrationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FProperty* ScoreboardProperty = FindFProperty<FProperty>(
		ALB_RaidGameState::StaticClass(),
		GET_MEMBER_NAME_CHECKED(ALB_RaidGameState, RaidScoreboardData));
	const FProperty* MVPProperty = FindFProperty<FProperty>(
		ALB_PlayerState::StaticClass(),
		TEXT("bIsMVP"));

	TestNotNull(TEXT("RaidScoreboardData is present in reflection data"), ScoreboardProperty);
	TestNotNull(TEXT("bIsMVP is present in reflection data"), MVPProperty);

	if (ScoreboardProperty)
	{
		TestTrue(TEXT("RaidScoreboardData has the network property flag"), ScoreboardProperty->HasAnyPropertyFlags(CPF_Net));
		TestTrue(TEXT("RaidScoreboardData has a RepNotify flag"), ScoreboardProperty->HasAnyPropertyFlags(CPF_RepNotify));
		TestTrue(
			TEXT("RaidScoreboardData is registered by GetLifetimeReplicatedProps"),
			IsLifetimeReplicated(GetDefault<ALB_RaidGameState>(), ScoreboardProperty));
	}

	if (MVPProperty)
	{
		TestTrue(TEXT("bIsMVP has the network property flag"), MVPProperty->HasAnyPropertyFlags(CPF_Net));
		TestTrue(
			TEXT("bIsMVP is registered by GetLifetimeReplicatedProps"),
			IsLifetimeReplicated(GetDefault<ALB_PlayerState>(), MVPProperty));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
