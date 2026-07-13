#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "GameMode/LB_RaidMVPUtils.h"
#include "GameState/LB_RaidGameState.h"
#include "Misc/AutomationTest.h"
#include "Net/UnrealNetwork.h"
#include "Player/LB_PlayerState.h"
#include "System/Raid/LBRaidDataRows.h"
#include "System/Raid/LBRaidTypes.h"
#include "UI/Result/LB_RaidScoreboardWidget.h"
#include "UI/Result/LB_RaidScoreSlotWidget.h"
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

	// CPF_Net만 검사하면 잘못된 OwnerOnly/InitialOnly 등록을 놓치므로 실제 lifetime 정책까지 조회한다.
	bool FindLifetimeReplicationPolicy(
		const AActor* DefaultObject,
		const FProperty* Property,
		FLifetimeProperty& OutLifetimeProperty)
	{
		if (!DefaultObject || !Property)
		{
			return false;
		}

		TArray<FLifetimeProperty> LifetimeProperties;
		DefaultObject->GetLifetimeReplicatedProps(LifetimeProperties);
		if (const FLifetimeProperty* LifetimeProperty = LifetimeProperties.FindByPredicate(
			[Property](const FLifetimeProperty& Candidate)
			{
				return Candidate.RepIndex == Property->RepIndex;
			}))
		{
			OutLifetimeProperty = *LifetimeProperty;
			return true;
		}

		return false;
	}

	void TestReplicationPolicy(
		FAutomationTestBase& Test,
		const AActor* DefaultObject,
		const UClass* ObjectClass,
		const FName PropertyName,
		const ELifetimeCondition ExpectedCondition,
		const bool bExpectedRepNotify)
	{
		const FString PropertyLabel = FString::Printf(TEXT("%s.%s"), *ObjectClass->GetName(), *PropertyName.ToString());
		const FProperty* Property = FindFProperty<FProperty>(ObjectClass, PropertyName);
		Test.TestNotNull(*FString::Printf(TEXT("%s is present in reflection data"), *PropertyLabel), Property);
		if (!Property)
		{
			return;
		}

		Test.TestTrue(
			*FString::Printf(TEXT("%s has the network property flag"), *PropertyLabel),
			Property->HasAnyPropertyFlags(CPF_Net));
		Test.TestEqual(
			*FString::Printf(TEXT("%s RepNotify flag matches its serialization contract"), *PropertyLabel),
			Property->HasAnyPropertyFlags(CPF_RepNotify),
			bExpectedRepNotify);

		FLifetimeProperty LifetimeProperty;
		const bool bFoundLifetimeProperty = FindLifetimeReplicationPolicy(DefaultObject, Property, LifetimeProperty);
		Test.TestTrue(
			*FString::Printf(TEXT("%s is registered by GetLifetimeReplicatedProps"), *PropertyLabel),
			bFoundLifetimeProperty);
		if (!bFoundLifetimeProperty)
		{
			return;
		}

		Test.TestEqual(
			*FString::Printf(TEXT("%s uses the intended replication condition"), *PropertyLabel),
			static_cast<uint8>(LifetimeProperty.Condition),
			static_cast<uint8>(ExpectedCondition));
		Test.TestEqual(
			*FString::Printf(TEXT("%s keeps the default change-only RepNotify policy"), *PropertyLabel),
			static_cast<uint8>(LifetimeProperty.RepNotifyCondition),
			static_cast<uint8>(REPNOTIFY_OnChanged));
	}

	void TestDataTableContract(
		FAutomationTestBase& Test,
		const TCHAR* AssetPath,
		const UScriptStruct* ExpectedRowStruct,
		const TConstArrayView<FName> RequiredRows)
	{
		// 자동화 테스트에서만 동기 로드해 패키징된 에셋과 C++ RowStruct 계약이 실제로 이어지는지 검증한다.
		const UDataTable* DataTable = LoadObject<UDataTable>(nullptr, AssetPath);
		Test.TestNotNull(*FString::Printf(TEXT("DataTable loads: %s"), AssetPath), DataTable);
		if (!DataTable)
		{
			return;
		}

		Test.TestTrue(
			*FString::Printf(TEXT("%s keeps its expected RowStruct"), AssetPath),
			DataTable->GetRowStruct() == ExpectedRowStruct);
		for (const FName RequiredRow : RequiredRows)
		{
			Test.TestTrue(
				*FString::Printf(TEXT("%s contains required row %s"), AssetPath, *RequiredRow.ToString()),
				DataTable->GetRowMap().Contains(RequiredRow));
		}
	}

	void TestStructPropertyOrder(
		FAutomationTestBase& Test,
		const UScriptStruct* Struct,
		const TConstArrayView<FName> ExpectedPropertyNames)
	{
		TArray<FName> ActualPropertyNames;
		for (TFieldIterator<FProperty> PropertyIt(Struct, EFieldIteratorFlags::ExcludeSuper); PropertyIt; ++PropertyIt)
		{
			ActualPropertyNames.Add(PropertyIt->GetFName());
		}

		Test.TestEqual(
			*FString::Printf(TEXT("%s keeps its serialized property count"), *Struct->GetName()),
			ActualPropertyNames.Num(),
			ExpectedPropertyNames.Num());
		const int32 ComparablePropertyCount = FMath::Min(ActualPropertyNames.Num(), ExpectedPropertyNames.Num());
		for (int32 Index = 0; Index < ComparablePropertyCount; ++Index)
		{
			Test.TestEqual(
				*FString::Printf(TEXT("%s serialized property %d keeps its name and order"), *Struct->GetName(), Index),
				ActualPropertyNames[Index],
				ExpectedPropertyNames[Index]);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBRaidScoreboardWidgetClassContractTest,
	"LeftBehind.Raid.Scoreboard.WidgetClassContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBRaidScoreboardWidgetClassContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const TCHAR* ScoreboardWidgetClassPath =
		TEXT("/Game/LeftBehind/UI/BattleHUD/Result/WBP_LB_RaidScoreboardWidget.WBP_LB_RaidScoreboardWidget_C");
	UClass* ScoreboardWidgetClass = LoadClass<ULB_RaidScoreboardWidget>(nullptr, ScoreboardWidgetClassPath);
	TestNotNull(TEXT("The raid scoreboard Widget Blueprint class loads"), ScoreboardWidgetClass);
	if (!ScoreboardWidgetClass)
	{
		return false;
	}

	const UObject* ScoreboardCDO = ScoreboardWidgetClass->GetDefaultObject();
	const FSoftClassProperty* SlotClassProperty = FindFProperty<FSoftClassProperty>(
		ScoreboardWidgetClass,
		TEXT("RaidScoreSlotClass"));
	TestNotNull(TEXT("The raid scoreboard exposes its soft slot class property"), SlotClassProperty);
	if (!SlotClassProperty || !ScoreboardCDO)
	{
		return false;
	}

	const FSoftObjectPath SlotClassPath =
		SlotClassProperty->GetPropertyValue_InContainer(ScoreboardCDO).ToSoftObjectPath();
	TestFalse(TEXT("The raid score slot class path is configured"), SlotClassPath.IsNull());
	UClass* LoadedSlotClass = Cast<UClass>(SlotClassPath.TryLoad());
	TestNotNull(TEXT("The configured raid score slot Widget Blueprint class loads"), LoadedSlotClass);
	if (LoadedSlotClass)
	{
		TestTrue(
			TEXT("The configured slot class uses the native raid score slot base"),
			LoadedSlotClass->IsChildOf(ULB_RaidScoreSlotWidget::StaticClass()));
	}

	return true;
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
	FLBRaidMVPBoundaryContractTest,
	"LeftBehind.Raid.Scoreboard.MVP.BoundaryContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBRaidMVPBoundaryContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const float MaxFloat = std::numeric_limits<float>::max();
	const float NaN = std::numeric_limits<float>::quiet_NaN();
	const TArray<LBRaidMVP::FScoringInput> MaxValueInputs = {
		MakeScoringInput(ELBRoleType::DPS, MaxFloat, 0.f, MAX_int32, 7),
		MakeScoringInput(ELBRoleType::Healer, 0.f, MaxFloat, 0, 8)
	};

	const LBRaidMVP::FScoringContext MaxValueContext = LBRaidMVP::BuildScoringContext(MaxValueInputs);
	const LBRaidMVP::FScoringResult MaxDPSResult = LBRaidMVP::CalculateScore(MaxValueInputs[0], MaxValueContext);
	TestTrue(TEXT("Maximum finite float input stays finite after normalization"), FMath::IsFinite(MaxDPSResult.Score));
	TestTrue(TEXT("Maximum finite float normalizes to one without overflow"), IsNearlyEqual(MaxDPSResult.PrimaryNormalized, 1.f));
	TestEqual(TEXT("The maximum death boundary remains unchanged"), LBRaidMVP::SanitizeDeathCount(MAX_int32), MAX_int32);
	TestEqual(TEXT("A negative death boundary is sanitized without underflow"), LBRaidMVP::SanitizeDeathCount(MIN_int32), 0);

	const ELBRoleType InvalidRole = static_cast<ELBRoleType>(MAX_uint8);
	const TArray<LBRaidMVP::FScoringInput> InvalidRoleInputs = {
		MakeScoringInput(InvalidRole, MaxFloat, MaxFloat, 0, 1)
	};
	const LBRaidMVP::FScoringResult InvalidRoleResult = LBRaidMVP::CalculateScore(
		InvalidRoleInputs[0],
		LBRaidMVP::BuildScoringContext(InvalidRoleInputs));
	TestTrue(TEXT("An unknown role produces no score"), IsNearlyEqual(InvalidRoleResult.Score, 0.f));
	TestEqual(TEXT("An unknown role cannot become MVP"), LBRaidMVP::SelectMVPIndex(InvalidRoleInputs), INDEX_NONE);

	LBRaidMVP::FScoringResult CandidateResult;
	CandidateResult.Score = 0.5002f;
	LBRaidMVP::FScoringResult BestResult;
	BestResult.Score = 0.5f;
	const LBRaidMVP::FScoringInput TieInput = MakeScoringInput(ELBRoleType::DPS, 1.f, 0.f, 0, 1);
	TestTrue(
		TEXT("A non-finite epsilon falls back to the deterministic default"),
		LBRaidMVP::IsBetterCandidate(TieInput, CandidateResult, 1, TieInput, BestResult, 0, NaN));
	TestTrue(
		TEXT("A negative epsilon is clamped to zero"),
		LBRaidMVP::IsBetterCandidate(TieInput, CandidateResult, 1, TieInput, BestResult, 0, -1.f));
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

	LBRaidMVP::FScoringInput CandidateWithMoreDeaths = TieInput;
	CandidateWithMoreDeaths.DeathCount = 100;
	LBRaidMVP::FScoringResult HigherScore = HigherPrimary;
	HigherScore.Score = 0.9f;
	TestTrue(
		TEXT("Final score has priority over every tie-break field"),
		LBRaidMVP::IsBetterCandidate(CandidateWithMoreDeaths, HigherScore, 1, TieInput, HigherPrimary, 0));

	LBRaidMVP::FScoringResult BetterSecondary = HigherPrimary;
	BetterSecondary.SecondaryNormalized = 0.5f;
	TestTrue(
		TEXT("Secondary contribution breaks a tie after score, deaths, and primary contribution"),
		LBRaidMVP::IsBetterCandidate(TieInput, BetterSecondary, 1, TieInput, HigherPrimary, 0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBRaidScoreboardReplicationRegistrationTest,
	"LeftBehind.Raid.Scoreboard.ReplicationRegistration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBRaidScoreboardReplicationRegistrationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const ALB_RaidGameState* RaidGameStateCDO = GetDefault<ALB_RaidGameState>();
	TestReplicationPolicy(
		*this,
		RaidGameStateCDO,
		ALB_RaidGameState::StaticClass(),
		GET_MEMBER_NAME_CHECKED(ALB_RaidGameState, RaidResult),
		COND_None,
		true);
	TestReplicationPolicy(
		*this,
		RaidGameStateCDO,
		ALB_RaidGameState::StaticClass(),
		GET_MEMBER_NAME_CHECKED(ALB_RaidGameState, RaidScoreboardData),
		COND_None,
		true);

	const ALB_PlayerState* PlayerStateCDO = GetDefault<ALB_PlayerState>();
	TestReplicationPolicy(*this, PlayerStateCDO, ALB_PlayerState::StaticClass(), TEXT("RoleType"), COND_None, true);
	TestReplicationPolicy(*this, PlayerStateCDO, ALB_PlayerState::StaticClass(), TEXT("DeathCount"), COND_None, true);
	TestReplicationPolicy(*this, PlayerStateCDO, ALB_PlayerState::StaticClass(), TEXT("bIsDead"), COND_None, true);
	TestReplicationPolicy(*this, PlayerStateCDO, ALB_PlayerState::StaticClass(), TEXT("CharacterID"), COND_None, true);
	TestReplicationPolicy(*this, PlayerStateCDO, ALB_PlayerState::StaticClass(), TEXT("bIsMVP"), COND_None, false);
	TestReplicationPolicy(*this, PlayerStateCDO, ALB_PlayerState::StaticClass(), TEXT("TotalDamageDealt"), COND_OwnerOnly, false);
	TestReplicationPolicy(*this, PlayerStateCDO, ALB_PlayerState::StaticClass(), TEXT("TotalHealingDone"), COND_OwnerOnly, false);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBRaidReplicationTuningTest,
	"LeftBehind.Raid.Scoreboard.ReplicationTuning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBRaidReplicationTuningTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const ALB_RaidGameState* RaidGameStateCDO = GetDefault<ALB_RaidGameState>();
	TestTrue(TEXT("RaidGameState replicates at 10 Hz"), IsNearlyEqual(RaidGameStateCDO->GetNetUpdateFrequency(), 10.f));
	TestTrue(TEXT("RaidGameState throttles down to 2 Hz"), IsNearlyEqual(RaidGameStateCDO->GetMinNetUpdateFrequency(), 2.f));
	TestTrue(TEXT("RaidGameState keeps high saturated-network priority"), IsNearlyEqual(RaidGameStateCDO->NetPriority, 10.f));

	const ALB_PlayerState* PlayerStateCDO = GetDefault<ALB_PlayerState>();
	TestTrue(TEXT("PlayerState replicates at 30 Hz"), IsNearlyEqual(PlayerStateCDO->GetNetUpdateFrequency(), 30.f));
	TestTrue(TEXT("PlayerState throttles down to 5 Hz"), IsNearlyEqual(PlayerStateCDO->GetMinNetUpdateFrequency(), 5.f));
	TestTrue(TEXT("PlayerState keeps the intended saturated-network priority"), IsNearlyEqual(PlayerStateCDO->NetPriority, 2.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBRaidEnumSerializationContractTest,
	"LeftBehind.Raid.Scoreboard.Serialization.EnumOrdinals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBRaidEnumSerializationContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TestEqual(TEXT("ELBRoleType::DPS ordinal remains 0"), static_cast<uint8>(ELBRoleType::DPS), static_cast<uint8>(0));
	TestEqual(TEXT("ELBRoleType::Healer ordinal remains 1"), static_cast<uint8>(ELBRoleType::Healer), static_cast<uint8>(1));

	TestEqual(TEXT("ELBCharacterID::None ordinal remains 0"), static_cast<uint8>(ELBCharacterID::None), static_cast<uint8>(0));
	TestEqual(TEXT("ELBCharacterID::Boris ordinal remains 1"), static_cast<uint8>(ELBCharacterID::Boris), static_cast<uint8>(1));
	TestEqual(TEXT("ELBCharacterID::Dekker ordinal remains 2"), static_cast<uint8>(ELBCharacterID::Dekker), static_cast<uint8>(2));
	TestEqual(TEXT("ELBCharacterID::Grux ordinal remains 3"), static_cast<uint8>(ELBCharacterID::Grux), static_cast<uint8>(3));
	TestEqual(TEXT("ELBCharacterID::IggyScorch ordinal remains 4"), static_cast<uint8>(ELBCharacterID::IggyScorch), static_cast<uint8>(4));

	TestEqual(TEXT("ELBRaidState::Waiting ordinal remains 0"), static_cast<uint8>(ELBRaidState::Waiting), static_cast<uint8>(0));
	TestEqual(TEXT("ELBRaidState::Countdown ordinal remains 1"), static_cast<uint8>(ELBRaidState::Countdown), static_cast<uint8>(1));
	TestEqual(TEXT("ELBRaidState::Battle ordinal remains 2"), static_cast<uint8>(ELBRaidState::Battle), static_cast<uint8>(2));
	TestEqual(TEXT("ELBRaidState::Result ordinal remains 3"), static_cast<uint8>(ELBRaidState::Result), static_cast<uint8>(3));

	TestEqual(TEXT("ELBRaidEndReason::None ordinal remains 0"), static_cast<uint8>(ELBRaidEndReason::None), static_cast<uint8>(0));
	TestEqual(TEXT("ELBRaidEndReason::BossKilled ordinal remains 1"), static_cast<uint8>(ELBRaidEndReason::BossKilled), static_cast<uint8>(1));
	TestEqual(TEXT("ELBRaidEndReason::AllDead ordinal remains 2"), static_cast<uint8>(ELBRaidEndReason::AllDead), static_cast<uint8>(2));
	TestEqual(TEXT("ELBRaidEndReason::TimeOut ordinal remains 3"), static_cast<uint8>(ELBRaidEndReason::TimeOut), static_cast<uint8>(3));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLBRaidDataTableSerializationContractTest,
	"LeftBehind.Raid.Scoreboard.Serialization.DataTables",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLBRaidDataTableSerializationContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FName BossRows[] = {TEXT("Boss_Proto_Test")};
	TestDataTableContract(
		*this,
		TEXT("/Game/LeftBehind/Data/System/DT_BossStats.DT_BossStats"),
		FLBBossStatsRow::StaticStruct(),
		MakeArrayView(BossRows));

	const FName RankRows[] = {TEXT("Rank_Adventurer"), TEXT("Rank_Beginner"), TEXT("Rank_Expert")};
	TestDataTableContract(
		*this,
		TEXT("/Game/LeftBehind/Data/System/DT_RankData.DT_RankData"),
		FLBRankDataRow::StaticStruct(),
		MakeArrayView(RankRows));

	const FName RoleRows[] = {TEXT("Role_DPS"), TEXT("Role_Healer")};
	TestDataTableContract(
		*this,
		TEXT("/Game/LeftBehind/Data/System/DT_RoleData.DT_RoleData"),
		FLBRoleData::StaticStruct(),
		MakeArrayView(RoleRows));

	const FName CharacterRows[] = {TEXT("Boris"), TEXT("Dekker"), TEXT("Grux"), TEXT("IggyScorch")};
	TestDataTableContract(
		*this,
		TEXT("/Game/LeftBehind/Data/System/DT_CharacterData.DT_CharacterData"),
		FLBCharacterData::StaticStruct(),
		MakeArrayView(CharacterRows));

	// 이름과 선언 순서를 고정해 기존 Blueprint 핀과 DataTable 열의 직렬화 계약을 함께 보호한다.
	const FName BossPropertyNames[] = {
		TEXT("BossID"), TEXT("BossClass"), TEXT("MaxHP"), TEXT("MaxMana"), TEXT("DEF"), TEXT("TimeLimitSec"), TEXT("Phase2ThresholdRatio")
	};
	TestStructPropertyOrder(*this, FLBBossStatsRow::StaticStruct(), MakeArrayView(BossPropertyNames));
	const FName RankPropertyNames[] = {TEXT("RankID"), TEXT("ClearTimeSec"), TEXT("Title"), TEXT("RewardID")};
	TestStructPropertyOrder(*this, FLBRankDataRow::StaticStruct(), MakeArrayView(RankPropertyNames));
	const FName RolePropertyNames[] = {TEXT("RoleName"), TEXT("RoleSubtitle"), TEXT("RoleIcon"), TEXT("RoleColor"), TEXT("RoleType")};
	TestStructPropertyOrder(*this, FLBRoleData::StaticStruct(), MakeArrayView(RolePropertyNames));
	const FName CharacterPropertyNames[] = {
		TEXT("DisplayName"), TEXT("CardPortraitImage"), TEXT("HUDPortraitImage"), TEXT("RoleType"),
		TEXT("PreviewMesh"), TEXT("PreviewAnimClass"), TEXT("bLocked"), TEXT("HuntingGrade"),
		TEXT("AttackPower"), TEXT("Defense"), TEXT("CriticalRate"), TEXT("MoveSpeed"),
		TEXT("PreviewRenderTarget"), TEXT("PreviewMaterialInstance"), TEXT("MeshOffset"),
		TEXT("MeshRotation"), TEXT("MeshScale")
	};
	TestStructPropertyOrder(*this, FLBCharacterData::StaticStruct(), MakeArrayView(CharacterPropertyNames));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
