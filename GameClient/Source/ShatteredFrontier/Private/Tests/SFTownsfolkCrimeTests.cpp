#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Townsfolk/SFTownsfolk.h"
#include "Characters/SFE1Character.h"
#include "Combat/SFCombatPilot.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

// Townsfolk step 5 (AI Townsfolk doc sections 5.5 and 9): residents fight back or flee, guards witness and engage,
// unwitnessed crimes are reported only when the victim reaches a guard.
namespace
{
    struct FCrimeWorld
    {
        FTestWorldWrapper Fixture; UWorld* World=nullptr; ASFTownState* State=nullptr; ASFTownGuard* Guard=nullptr;
        bool Start()
        {
            if (!Fixture.CreateTestWorld(EWorldType::Game) || !Fixture.BeginPlayInTestWorld()) { return false; }
            World=Fixture.GetTestWorld();
            State=World->SpawnActor<ASFTownState>(); State->Start(*GetDefault<USFTownsfolkSettings>());
            Guard=World->SpawnActor<ASFTownGuard>(FVector(0,0,100),FRotator::ZeroRotator);
            return State && Guard;
        }
        ASFE1Character* Player(double X)
        {
            FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            ASFE1Character* P=World->SpawnActor<ASFE1Character>(FVector(X,0,100),FRotator::ZeroRotator,Spawn);
            P->GetCharacterMovement()->DisableMovement();  // no floor in the test world
            P->CombatSetHealth(GetDefault<USFCombatPilotSettings>()->PlayerHealth);
            return P;
        }
        ASFTownsfolkNpc* Resident(const TCHAR* Id, double X)
        {
            const FTransform Where(FVector(X,0,100));
            ASFTownsfolkNpc* Npc=World->SpawnActorDeferred<ASFTownsfolkNpc>(ASFTownsfolkNpc::StaticClass(),Where,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            Npc->Init(Id,TEXT("labourer"),1,40,false);
            Npc->FinishSpawning(Where);
            return Npc;
        }
        void Run(float Seconds) { for (float T=0; T<Seconds; T+=0.05f) { Fixture.TickTestWorld(0.05f); } }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFTownsfolkCrimeRulesTest,
    "ShatteredFrontier.Townsfolk.Crime.Rules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFTownsfolkCrimeRulesTest::RunTest(const FString& Parameters)
{
    FString Error;
    TestTrue(TEXT("Shipped townsfolk settings are bounded"), GetDefault<USFTownsfolkSettings>()->Validate(Error));
    TestTrue(TEXT("Half health fights back"), SFTownCrime::FightsBack(20,40,35));
    TestFalse(TEXT("At the flee share it flees"), SFTownCrime::FightsBack(14,40,35));
    TestFalse(TEXT("The dead don't fight"), SFTownCrime::FightsBack(0,40,0));

    FCrimeWorld W; if (!W.Start()) { return false; }
    const AActor* A=W.World->SpawnActor<AActor>(); const AActor* B=W.World->SpawnActor<AActor>();
    TArray<FSFTownOffender> List;
    TestEqual(TEXT("A pawn without a player is its own key"), SFTownCrime::OffenderKey(A), A);
    SFTownCrime::Mark(List,A,100);
    TestTrue(TEXT("Marked offender"), SFTownCrime::IsOffender(List,A,50));
    TestFalse(TEXT("Others are not"), SFTownCrime::IsOffender(List,B,50));
    TestFalse(TEXT("The grudge expires"), SFTownCrime::IsOffender(List,A,100));
    SFTownCrime::Mark(List,A,60);
    TestTrue(TEXT("A shorter mark never shortens the grudge"), SFTownCrime::IsOffender(List,A,99));
    TestEqual(TEXT("One entry per offender"), List.Num(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFTownsfolkCrimeGuardTest,
    "ShatteredFrontier.Townsfolk.Crime.GuardResponse",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFTownsfolkCrimeGuardTest::RunTest(const FString& Parameters)
{
    FCrimeWorld W; if (!W.Start()) { return false; }
    ASFE1Character* P=W.Player(300); ASFTownsfolkNpc* A=W.Resident(TEXT("test_a"),420);
    const int32 Full=P->CombatHealth();
    A->ReceiveHit(P,20);
    TestTrue(TEXT("Attacked at half health: fights back"), A->IsFighting());
    TestTrue(TEXT("A guard in sight witnesses: offender at once"), W.State->IsOffender(P));
    TestFalse(TEXT("Nothing left to report"), A->IsReporting());
    W.Run(3.0f);
    TestEqual(TEXT("The guard engages the offender"), W.Guard->Engaged(), P);
    TestTrue(TEXT("The guard strikes"), W.Guard->Hits>0);
    TestTrue(TEXT("The offender takes the guard's and the resident's blows"), P->CombatHealth()<=Full-GetDefault<USFTownsfolkSettings>()->GuardMeleeDamage-GetDefault<USFTownsfolkSettings>()->ResidentMeleeDamage);
    // An innocent player in plain view is left alone.
    FCrimeWorld W2; if (!W2.Start()) { return false; }
    ASFE1Character* Innocent=W2.Player(300);
    W2.Run(2.0f);
    TestNull(TEXT("No crime, no engagement"), W2.Guard->Engaged());
    TestEqual(TEXT("Innocent unharmed"), Innocent->CombatHealth(), Full);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFTownsfolkCrimeReportTest,
    "ShatteredFrontier.Townsfolk.Crime.Report",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFTownsfolkCrimeReportTest::RunTest(const FString& Parameters)
{
    FCrimeWorld W; if (!W.Start()) { return false; }
    const USFTownsfolkSettings* S=GetDefault<USFTownsfolkSettings>();
    // 50 m from the guard: out of sight, within report range. Badly hurt, B flees straight to the guard.
    ASFE1Character* P1=W.Player(5000); ASFTownsfolkNpc* B=W.Resident(TEXT("test_b"),5120);
    B->ReceiveHit(P1,30);
    TestFalse(TEXT("Badly hurt: no fight"), B->IsFighting());
    TestTrue(TEXT("Unwitnessed: runs to report"), B->IsReporting());
    TestFalse(TEXT("Not an offender until the guard hears"), W.State->IsOffender(P1));
    // 200 m from any guard: unreported. C fights back.
    ASFE1Character* P2=W.Player(20000); ASFTownsfolkNpc* C=W.Resident(TEXT("test_c"),20120);
    C->ReceiveHit(P2,20);
    TestFalse(TEXT("No guard in range: unreported"), C->IsReporting());
    // 40 m out, killed outright: its report dies with it.
    ASFE1Character* P3=W.Player(-4000); ASFTownsfolkNpc* D=W.Resident(TEXT("test_d"),-4120);
    D->ReceiveHit(P3,40);
    TestFalse(TEXT("The dead report nothing"), D->IsReporting());
    W.Run(4.0f);
    TestFalse(TEXT("Still on the way: the report takes the travel time"), W.State->IsOffender(P1));
    TestTrue(TEXT("C hit back"), P2->CombatHealth()<GetDefault<USFCombatPilotSettings>()->PlayerHealth);
    W.Run(50.0f/S->FleeSpeedMetresPerSecond+2.0f);
    TestTrue(TEXT("Reached the guard: offender"), W.State->IsOffender(P1));
    TestFalse(TEXT("Report delivered"), B->IsReporting());
    TestFalse(TEXT("Unreported crime leaves no offender"), W.State->IsOffender(P2));
    TestFalse(TEXT("Nor does a killed lone witness"), W.State->IsOffender(P3));
    TestNull(TEXT("A reported offender out of sight isn't engaged"), W.Guard->Engaged());
    return true;
}
#endif
