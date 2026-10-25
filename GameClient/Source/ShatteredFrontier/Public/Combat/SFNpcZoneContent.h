#pragma once

#include "CoreMinimal.h"
class UWorld;

namespace SFNpcZoneContent
{
    struct FGroup
    {
        TArray<FVector2D> Polygon;
        TSet<FString> Definitions;
        TMap<FString, bool> DefensivePoints;
        int32 ActiveCount = 0;
    };
    struct FZone
    {
        TMap<FString, FGroup> Groups;
        TArray<TArray<FVector2D>> ProtectedAreas;
        TArray<FVector> RespawnPoints;
    };

    /** Immutable index of approved release zone records; no map-name branches. */
    SHATTEREDFRONTIER_API FString FindMapFile(const FString& MapId);
    SHATTEREDFRONTIER_API bool IsProductionMap(const UWorld* World);
    /** Parse the selected map's NPC groups against the validated profile catalogue, atomically. */
    SHATTEREDFRONTIER_API bool Load(const FString& Path, const FString& MapId, FZone& Out, FString& Error);
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    class SHATTEREDFRONTIER_API FScopedTestMap
    {
    public:
        FScopedTestMap(const FString& MapId, const FString& Path);
        ~FScopedTestMap();
        FScopedTestMap(const FScopedTestMap&) = delete;
        FScopedTestMap& operator=(const FScopedTestMap&) = delete;
    private:
        TMap<FString, FString> Maps;
        const TMap<FString, FString>* Previous;
    };
#endif
}
