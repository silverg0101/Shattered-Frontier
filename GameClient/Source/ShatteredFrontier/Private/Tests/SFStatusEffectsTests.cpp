#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Combat/SFStatusEffectsComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/WorldSettings.h"

namespace
{
struct FStatusWorld
{
    FTestWorldWrapper Fixture;
    bool Begin(FAutomationTestBase& Test)
    {
        if (!Fixture.CreateTestWorld(EWorldType::Game) || !Fixture.BeginPlayInTestWorld())
        { Fixture.ForwardErrorMessages(&Test); return false; }
        Fixture.GetTestWorld()->GetWorldSettings()->NotifyBeginPlay();
        return true;
    }
    USFStatusEffectsComponent* Spawn(ACharacter*& OutOwner)
    {
        OutOwner = Fixture.GetTestWorld()->SpawnActor<ACharacter>(ACharacter::StaticClass());
        auto* Effects = NewObject<USFStatusEffectsComponent>(OutOwner);
        Effects->RegisterComponent();
        return Effects;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFStatusEffectsLifecycleTest,
    "ShatteredFrontier.Combat.StatusEffects.ApplyTickStackRefreshExpire",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFStatusEffectsLifecycleTest::RunTest(const FString& Parameters)
{
    FStatusWorld World;
    if (!World.Begin(*this)) return false;
    ACharacter *Target = nullptr, *Caster = nullptr;
    auto* Effects = World.Spawn(Target);
    World.Spawn(Caster);
    int32 Ticks = 0;
    auto Apply = [&]() { return Effects->Apply(TEXT("effect/weak_poison"), ESFStatusKind::Poison,
        ESFDispelType::Poison, Caster, 6.f, 2.f, 3, true, [&Ticks]() { ++Ticks; }); };
    TestFalse(TEXT("rejects unbounded tick frequency"), Effects->Apply(TEXT("invalid"),
        ESFStatusKind::Poison, ESFDispelType::Poison, Caster, 6.f, .001f, 3, true,
        [&Ticks]() { ++Ticks; }));
    TestTrue(TEXT("applies on authority"), Apply());
    TestEqual(TEXT("first stack"), Effects->GetStacks(TEXT("effect/weak_poison"), Caster), 1);
    Effects->Advance(2.f);
    TestEqual(TEXT("first tick"), Ticks, 1);
    TestTrue(TEXT("second stack"), Apply());
    TestTrue(TEXT("third stack"), Apply());
    TestTrue(TEXT("stack cap"), Apply());
    TestEqual(TEXT("three stacks"), Effects->GetStacks(TEXT("effect/weak_poison"), Caster), 3);
    TestTrue(TEXT("refresh capped at 1.3 times duration"),
        Effects->GetDisplayEffects()[0].ExpiresAtServerSeconds <= 2.0 + 7.8 + KINDA_SMALL_NUMBER);
    Effects->Advance(2.f);
    TestEqual(TEXT("independent tick per stack"), Ticks, 4);
    Effects->Advance(6.f);
    TestEqual(TEXT("expired"), Effects->GetStacks(TEXT("effect/weak_poison"), Caster), 0);
    TestTrue(TEXT("replicated view clears"), Effects->GetDisplayEffects().IsEmpty());
    int32 DeathTicks = 0;
    Effects->Apply(TEXT("death_cleanup"), ESFStatusKind::Poison, ESFDispelType::Poison,
        Caster, 6.f, 2.f, 3, true, [&]() { ++DeathTicks; Effects->ClearAll(); });
    Effects->Apply(TEXT("death_cleanup"), ESFStatusKind::Poison, ESFDispelType::Poison,
        Caster, 6.f, 2.f, 3, true, [&]() { ++DeathTicks; Effects->ClearAll(); });
    Effects->Advance(2.f);
    TestEqual(TEXT("death cleanup stops remaining stack ticks"), DeathTicks, 1);
    TestTrue(TEXT("death cleanup clears display"), Effects->GetDisplayEffects().IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFStatusEffectsDispelTest,
    "ShatteredFrontier.Combat.StatusEffects.PurifyAndReplicationContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFStatusEffectsDispelTest::RunTest(const FString& Parameters)
{
    FStatusWorld World;
    if (!World.Begin(*this)) return false;
    ACharacter *Target = nullptr, *Caster = nullptr;
    auto* Effects = World.Spawn(Target);
    World.Spawn(Caster);
    auto Add = [&](FName Id, ESFStatusKind Kind, ESFDispelType Dispel)
    { return Effects->Apply(Id, Kind, Dispel, Caster, 10.f, 0.f, 1, false); };
    TestTrue(TEXT("physical bleed"), Add(TEXT("bleed"), ESFStatusKind::Bleed, ESFDispelType::None));
    TestTrue(TEXT("magic"), Add(TEXT("magic"), ESFStatusKind::Magic, ESFDispelType::Magic));
    TestTrue(TEXT("poison"), Add(TEXT("poison"), ESFStatusKind::Poison, ESFDispelType::Poison));
    TestTrue(TEXT("disease"), Add(TEXT("disease"), ESFStatusKind::Disease, ESFDispelType::Disease));
    TestTrue(TEXT("curse"), Add(TEXT("curse"), ESFStatusKind::Curse, ESFDispelType::Curse));
    TestEqual(TEXT("display snapshots contain five"), Effects->GetDisplayEffects().Num(), 5);
    TestTrue(TEXT("newest eligible disease first"), Effects->PurifyNewest());
    TestEqual(TEXT("disease removed"), Effects->GetStacks(TEXT("disease"), Caster), 0);
    TestTrue(TEXT("next poison"), Effects->PurifyNewest());
    TestEqual(TEXT("poison removed"), Effects->GetStacks(TEXT("poison"), Caster), 0);
    TestTrue(TEXT("then magic"), Effects->PurifyNewest());
    TestFalse(TEXT("curse and bleed are protected"), Effects->PurifyNewest());
    TestEqual(TEXT("curse remains"), Effects->GetStacks(TEXT("curse"), Caster), 1);
    TestEqual(TEXT("bleed remains"), Effects->GetStacks(TEXT("bleed"), Caster), 1);
    const FProperty* View = FindFProperty<FProperty>(USFStatusEffectsComponent::StaticClass(), TEXT("DisplayEffects"));
    TestTrue(TEXT("display array is a replicated property"), View && View->HasAnyPropertyFlags(CPF_Net));
    FString Error;
    TestTrue(TEXT("shipped refresh tuning is valid"), GetDefault<USFStatusEffectsSettings>()->Validate(Error));
    USFStatusEffectsSettings* Invalid = NewObject<USFStatusEffectsSettings>();
    Invalid->WeakPoisonTicks = 0;
    TestFalse(TEXT("zero-tick poison is rejected"), Invalid->Validate(Error));
    Invalid->WeakPoisonTicks = 3;
    Invalid->FrostSlowMultiplier = .2f;
    TestFalse(TEXT("slow beyond the movement cap is rejected"), Invalid->Validate(Error));
    return true;
}
#endif
