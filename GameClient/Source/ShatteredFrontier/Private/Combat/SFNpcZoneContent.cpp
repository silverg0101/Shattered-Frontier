#include "Combat/SFNpcZoneContent.h"
#include "Combat/SFNpcProfiles.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    thread_local const TMap<FString, FString>* TestMaps = nullptr;
#endif
    bool Read(const FString& Path, TSharedPtr<FJsonObject>& Root)
    {
        FString Source;
        return FFileHelper::LoadFileToString(Source, *Path) &&
            FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Source), Root) && Root.IsValid();
    }
    bool Token(const FString& Value)
    {
        if (Value.IsEmpty()) return false;
        for (const TCHAR C : Value)
            if (!(C >= 'a' && C <= 'z') && !(C >= '0' && C <= '9') && C != '_') return false;
        return true;
    }
    bool Point(const TSharedPtr<FJsonValue>& Value, FVector& Out)
    {
        const TArray<TSharedPtr<FJsonValue>>* Pair = nullptr;
        double X, Y, Z = 0;
        if (!Value->TryGetArray(Pair) || Pair->Num() < 2 ||
            !(*Pair)[0]->TryGetNumber(X) || !(*Pair)[1]->TryGetNumber(Y) ||
            (Pair->Num() > 2 && !(*Pair)[2]->TryGetNumber(Z)) ||
            !FMath::IsFinite(X * 100) || !FMath::IsFinite(Y * 100) || !FMath::IsFinite(Z * 100)) return false;
        Out = FVector(X, Y, Z);
        return true;
    }
    bool Polygon(const TArray<TSharedPtr<FJsonValue>>& Values, TArray<FVector2D>& Out)
    {
        if (Values.Num() < 3) return false;
        for (const auto& Value : Values)
        {
            FVector P;
            if (!Point(Value, P)) return false;
            Out.Emplace(P.X, P.Y);
        }
        return true;
    }
}

FString SFNpcZoneContent::FindMapFile(const FString& MapId)
{
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    if (TestMaps) { const auto* Path = TestMaps->Find(MapId); return Path ? *Path : FString(); }
#endif
    static const TMap<FString, FString> FilesByMap = []
    {
        const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), TEXT("../content/world/zones"));
        TArray<FString> Files;
        IFileManager::Get().FindFiles(Files, *(Directory / TEXT("*.json")), true, false);
        Files.Sort();
        TMap<FString, FString> Index;
        for (const FString& File : Files)
        {
            TSharedPtr<FJsonObject> Root;
            FString Id;
            if (!Read(Directory / File, Root) || !Root->TryGetStringField(TEXT("map_id"), Id) ||
                !Id.StartsWith(TEXT("/Game/Frontier/"), ESearchCase::CaseSensitive) || Index.Contains(Id))
            {
                UE_LOG(LogTemp, Error, TEXT("NPC_ZONE_INDEX_REJECTED File=%s"), *File);
                return TMap<FString, FString>();
            }
            Index.Add(Id, Directory / File);
        }
        return Index;
    }();
    const FString* Path = FilesByMap.Find(MapId);
    return Path ? *Path : FString();
}

bool SFNpcZoneContent::IsProductionMap(const UWorld* World)
{
    if (!World) return false;
    const FString MapId = World->GetOutermost()->GetName();
    return !MapId.StartsWith(TEXT("/Game/Frontier/Maps/Sandbox/"), ESearchCase::CaseSensitive) && !FindMapFile(MapId).IsEmpty();
}

bool SFNpcZoneContent::Load(const FString& Path, const FString& MapId, FZone& Out, FString& Error)
{
    Error = TEXT("Invalid NPC zone record");
    TSharedPtr<FJsonObject> Root;
    FString ActualMap;
    const TArray<TSharedPtr<FJsonValue>>* Groups = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* Areas = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* Respawns = nullptr;
    if (!Read(Path, Root) || !Root->TryGetStringField(TEXT("map_id"), ActualMap) || ActualMap != MapId ||
        !Root->TryGetArrayField(TEXT("spawn_groups"), Groups) || !Root->TryGetArrayField(TEXT("areas"), Areas) ||
        !Root->TryGetArrayField(TEXT("respawn_points"), Respawns)) return false;
    FZone Candidate;
    for (const auto& Value : *Groups)
    {
        const auto Group = Value->AsObject();
        FString Kind, Id;
        if (!Group.IsValid() || !Group->TryGetStringField(TEXT("spawn_kind"), Kind)) return false;
        if (Kind != TEXT("npc")) { if (Kind != TEXT("gathering_node")) return false; continue; }
        const TArray<TSharedPtr<FJsonValue>>* Definitions = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Area = nullptr;
        if (!Group->TryGetStringField(TEXT("group_id"), Id) || !Token(Id) || Candidate.Groups.Contains(Id) ||
            !Group->TryGetArrayField(TEXT("definitions"), Definitions) || Definitions->IsEmpty() ||
            !Group->TryGetArrayField(TEXT("points"), Points) || Points->IsEmpty() ||
            !Group->TryGetArrayField(TEXT("area"), Area)) return false;
        FGroup Record;
        double ActiveCount;
        if (!Group->TryGetNumberField(TEXT("active_count"), ActiveCount) || !FMath::IsFinite(ActiveCount) ||
            ActiveCount < 1 || ActiveCount > Points->Num() || FMath::FloorToDouble(ActiveCount) != ActiveCount) return false;
        Record.ActiveCount = static_cast<int32>(ActiveCount);
        if (!Polygon(*Area, Record.Polygon)) return false;
        for (const auto& Entry : *Definitions)
        {
            const auto Definition = Entry->AsObject();
            FString DefinitionId;
            double Weight;
            if (!Definition.IsValid() || !Definition->TryGetStringField(TEXT("definition_id"), DefinitionId) ||
                !SFNpcProfiles::Find(DefinitionId) || Record.Definitions.Contains(DefinitionId) ||
                !Definition->TryGetNumberField(TEXT("weight"), Weight) || !FMath::IsFinite(Weight) ||
                Weight <= 0 || Weight > MAX_int32 || FMath::FloorToDouble(Weight) != Weight)
            { Error = FString::Printf(TEXT("Invalid NPC definition in group %s"), *Id); return false; }
            Record.Definitions.Add(DefinitionId);
        }
        for (const auto& Entry : *Points)
        {
            const auto Spawn = Entry->AsObject();
            FString PointId;
            bool bDefensive = false;
            const TSharedPtr<FJsonValue>* Position = nullptr;
            FVector Location;
            if (!Spawn.IsValid() || !Spawn->TryGetStringField(TEXT("point_id"), PointId) || !Token(PointId) ||
                Record.DefensivePoints.Contains(PointId) || !(Position = Spawn->Values.Find(TEXT("position"))) ||
                !Point(*Position, Location) || (Spawn->HasField(TEXT("npc_defensive")) &&
                !Spawn->TryGetBoolField(TEXT("npc_defensive"), bDefensive))) return false;
            Record.DefensivePoints.Add(PointId, bDefensive);
        }
        Candidate.Groups.Add(Id, MoveTemp(Record));
    }
    for (const auto& Value : *Areas)
    {
        const auto Area = Value->AsObject();
        FString Safety;
        if (!Area.IsValid() || !Area->TryGetStringField(TEXT("safety"), Safety)) return false;
        if (Safety != TEXT("protected")) continue;
        const TArray<TSharedPtr<FJsonValue>>* PolygonValues = nullptr;
        if (Area->TryGetArrayField(TEXT("polygon"), PolygonValues))
        {
            TArray<FVector2D> Shape;
            if (!Polygon(*PolygonValues, Shape)) return false;
            Candidate.ProtectedAreas.Add(MoveTemp(Shape));
        }
        else if (Area->TryGetArrayField(TEXT("polygons"), PolygonValues))
        {
            for (const auto& ShapeValue : *PolygonValues)
            {
                const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
                TArray<FVector2D> Shape;
                if (!ShapeValue->TryGetArray(Points) || !Polygon(*Points, Shape)) return false;
                Candidate.ProtectedAreas.Add(MoveTemp(Shape));
            }
        }
        else return false;
    }
    for (const auto& Value : *Respawns)
    {
        const auto Spawn = Value->AsObject();
        const TSharedPtr<FJsonValue>* Position = nullptr;
        FVector Location;
        if (!Spawn.IsValid() || !(Position = Spawn->Values.Find(TEXT("position"))) || !Point(*Position, Location)) return false;
        Candidate.RespawnPoints.Add(Location * 100);
    }
    if (Candidate.Groups.IsEmpty() || Candidate.RespawnPoints.IsEmpty()) return false;
    Out = MoveTemp(Candidate);
    Error.Empty();
    return true;
}

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
SFNpcZoneContent::FScopedTestMap::FScopedTestMap(const FString& MapId, const FString& Path) : Previous(TestMaps)
{
    check(IsInGameThread());
    FZone Zone;
    FString Error;
    checkf(Load(Path, MapId, Zone, Error), TEXT("Invalid automation map fixture: %s"), *Error);
    Maps.Add(MapId, Path);
    TestMaps = &Maps;
}
SFNpcZoneContent::FScopedTestMap::~FScopedTestMap()
{
    check(IsInGameThread());
    TestMaps = Previous;
}
#endif
