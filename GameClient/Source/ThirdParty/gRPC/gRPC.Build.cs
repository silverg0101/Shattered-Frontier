using System;
using System.IO;
using System.Linq;
using UnrealBuildTool;

// Pinned gRPC C++ (with protobuf, abseil, re2, c-ares) built by tools/Build-GrpcThirdParty.ps1 into the
// git-ignored Artifacts/ThirdParty/gRPC/install/<Platform>. Pins: gRPC.lock.json beside this file.
// Only the server-only ShatteredFrontierBackend module depends on this.
public class gRPC : ModuleRules
{
    public gRPC(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;
        if (Target.Platform != UnrealTargetPlatform.Win64)
        {
            // INT-03 step 0b adds the Linux x86_64 build (UE v26 clang/libc++).
            throw new BuildException("gRPC ThirdParty is built for Win64 only; see docs/decisions/INT-03-gathering-ledger-slice.md step 0b.");
        }
        string RepoRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "..", ".."));
        string Install = Path.Combine(RepoRoot, "Artifacts", "ThirdParty", "gRPC", "install", "Win64");
        string Manifest = Path.Combine(Install, "manifest.json");
        if (!File.Exists(Manifest))
        {
            throw new BuildException("gRPC libraries are missing; run tools/Build-GrpcThirdParty.ps1 (expected " + Manifest + ").");
        }
        string LockText = File.ReadAllText(Path.Combine(ModuleDirectory, "gRPC.lock.json"));
        string ManifestText = File.ReadAllText(Manifest);
        string Commit = LockText.Split(new[] { "\"commit\": \"" }, StringSplitOptions.None)[1].Substring(0, 40);
        if (!ManifestText.Contains(Commit))
        {
            throw new BuildException("gRPC install does not match gRPC.lock.json; rerun tools/Build-GrpcThirdParty.ps1.");
        }

        PublicSystemIncludePaths.Add(Path.Combine(Install, "include"));
        // Link everything the install produced except variants that duplicate grpc/grpc++/protobuf symbols
        // and codegen-only libraries.
        string[] Excluded = { "grpc_unsecure.lib", "grpc++_unsecure.lib", "libprotobuf-lite.lib", "libprotoc.lib", "grpc_plugin_support.lib" };
        foreach (string Lib in Directory.GetFiles(Path.Combine(Install, "lib"), "*.lib").OrderBy(p => p))
        {
            if (!Excluded.Contains(Path.GetFileName(Lib), StringComparer.OrdinalIgnoreCase)) { PublicAdditionalLibraries.Add(Lib); }
        }
        PublicSystemLibraries.AddRange(new[] { "ws2_32.lib", "iphlpapi.lib", "advapi32.lib", "dbghelp.lib", "bcrypt.lib", "crypt32.lib" });

        // TLS and zlib come from the engine (one copy per process); keep in step with Get-TlsCMakeArgs.
        PublicDependencyModuleNames.AddRange(new[] { "OpenSSL", "zlib" });
        PublicDefinitions.Add("SF_WITH_GRPC=1");
    }
}
