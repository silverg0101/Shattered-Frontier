using System;
using System.IO;
using System.Linq;
using UnrealBuildTool;

// Pinned gRPC C++ (with protobuf, abseil, re2, c-ares) in the git-ignored Artifacts/ThirdParty/gRPC/install/<Platform>.
// Win64: tools/Build-GrpcThirdParty.ps1, TLS from the engine's OpenSSL (loopback development only; INT-03 hard gate).
// Linux: tools/build-grpc-linux.sh, TLS from BoringSSL prefixed sfbssl_* so it cannot clash with engine OpenSSL.
// Pins: gRPC.lock.json beside this file. Only the server-only ShatteredFrontierBackend module depends on this.
public class gRPC : ModuleRules
{
    public gRPC(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;
        bool bLinux = Target.Platform == UnrealTargetPlatform.Linux;
        if (Target.Platform != UnrealTargetPlatform.Win64 && !(bLinux && Target.Architecture == UnrealArch.X64))
        {
            throw new BuildException("gRPC ThirdParty is built for Win64 and Linux x86_64 only; see docs/decisions/INT-03-gathering-ledger-slice.md.");
        }
        string RepoRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "..", ".."));
        string Install = Path.Combine(RepoRoot, "Artifacts", "ThirdParty", "gRPC", "install", bLinux ? "Linux" : "Win64");
        string Manifest = Path.Combine(Install, "manifest.json");
        string Builder = bLinux ? "tools/build-grpc-linux.sh" : "tools/Build-GrpcThirdParty.ps1";
        if (!File.Exists(Manifest))
        {
            throw new BuildException("gRPC libraries are missing; run " + Builder + " (expected " + Manifest + ").");
        }
        string LockText = File.ReadAllText(Path.Combine(ModuleDirectory, "gRPC.lock.json"));
        string ManifestText = File.ReadAllText(Manifest);
        string Commit = LockText.Split(new[] { "\"commit\": \"" }, StringSplitOptions.None)[1].Substring(0, 40);
        if (!ManifestText.Contains(Commit) || (bLinux && !ManifestText.Contains("PrefixedBoringSSL")))
        {
            throw new BuildException("gRPC install does not match gRPC.lock.json; rerun " + Builder + ".");
        }

        PublicSystemIncludePaths.Add(Path.Combine(Install, "include"));
        // Link everything the install produced except variants that duplicate grpc/grpc++/protobuf symbols
        // and codegen-only libraries.
        string[] Excluded = { "grpc_unsecure", "grpc++_unsecure", "libprotobuf-lite", "libprotoc", "grpc_plugin_support" };
        foreach (string Lib in Directory.GetFiles(Path.Combine(Install, "lib"), bLinux ? "*.a" : "*.lib").OrderBy(p => p))
        {
            string Name = Path.GetFileNameWithoutExtension(Lib);
            string Bare = Name.StartsWith("lib") ? Name.Substring(3) : Name;  // Linux archives are lib<name>.a
            if (!Excluded.Contains(Name) && !Excluded.Contains(Bare)) { PublicAdditionalLibraries.Add(Lib); }
        }
        if (bLinux)
        {
            PublicSystemLibraries.AddRange(new[] { "pthread", "dl", "m" });
            // The install's libssl.a/libcrypto.a are the prefixed BoringSSL; no engine OpenSSL dependency here.
            PublicDependencyModuleNames.Add("zlib");
        }
        else
        {
            PublicSystemLibraries.AddRange(new[] { "ws2_32.lib", "iphlpapi.lib", "advapi32.lib", "dbghelp.lib", "bcrypt.lib", "crypt32.lib" });
            // TLS and zlib come from the engine (one copy per process); keep in step with Get-TlsCMakeArgs.
            PublicDependencyModuleNames.AddRange(new[] { "OpenSSL", "zlib" });
        }
        PublicDefinitions.Add("SF_WITH_GRPC=1");
    }
}
