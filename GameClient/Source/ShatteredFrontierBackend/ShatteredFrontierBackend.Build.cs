using UnrealBuildTool;

// Server-only (uproject Type "ServerOnly"): never built into the Client target. Holds the generated
// ledger.proto stubs and the gRPC transport for FSFInventoryLedgerClient.
public class ShatteredFrontierBackend : ModuleRules
{
    public ShatteredFrontierBackend(ReadOnlyTargetRules Target) : base(Target)
    {
        // Generated protobuf code and gRPC headers stay out of shared PCHs and unity blobs.
        PCHUsage = PCHUsageMode.NoPCHs;
        bUseUnity = false;
        bUseRTTI = true;  // libprotobuf is built with RTTI
        PrivateIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "Private", "Generated"));
        PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "Sockets", "Networking", "ShatteredFrontier", "gRPC" });
    }
}
