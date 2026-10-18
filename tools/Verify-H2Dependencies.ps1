[CmdletBinding()]
param([Parameter(Mandatory)][string]$EvidenceRoot)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$allowed = Join-Path $repo 'Artifacts/Persistence/DependencyLock'
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
if (-not $EvidenceRoot.StartsWith($allowed + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
    (Test-Path -LiteralPath $EvidenceRoot)) { throw 'Supply a new child of Artifacts/Persistence/DependencyLock.' }
$pins = Get-Content -LiteralPath (Join-Path $repo 'BackendServices/toolchain-lock.json') -Raw | ConvertFrom-Json
$cache = Join-Path $repo 'Artifacts/Persistence/Toolchains'
$dotnet = Join-Path $cache 'dotnet-10.0.402/dotnet.exe'
if ((Get-FileHash -LiteralPath $dotnet -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pins.sdk.executableSha256) { throw 'SDK host hash mismatch; prepare pinned toolchains first.' }
New-Item -ItemType Directory -Path $EvidenceRoot | Out-Null
$packages = Join-Path $cache 'nuget'
$env:DOTNET_CLI_TELEMETRY_OPTOUT = '1'
$env:DOTNET_GENERATE_ASPNET_CERTIFICATE = 'false'
$env:DOTNET_CLI_UI_LANGUAGE = 'en'
$projects = @('building-service/building-service.csproj', 'Shared/DependencyProbe/DependencyProbe.csproj', 'fixtures/PartitionProxy/PartitionProxy.csproj')
function Invoke-Dotnet([string[]]$CommandArgs, [string]$Log) {
    $output = & $dotnet @CommandArgs 2>&1
    $exitCode = $LASTEXITCODE
    $output | Set-Content -LiteralPath (Join-Path $EvidenceRoot $Log) -Encoding utf8
    if ($exitCode -ne 0) { throw "dotnet command failed; see $Log (exit $exitCode)." }
    return ($output -join "`n")
}
Push-Location (Join-Path $repo 'BackendServices')
try {
    $version = (& $dotnet --version).Trim()
    if ($version -ne $pins.sdk.version) { throw 'Unexpected SDK version.' }
    for ($i = 0; $i -lt $projects.Count; $i++) {
        $project = $projects[$i]
        Invoke-Dotnet @('restore', $project, '--locked-mode', '--packages', $packages, '--verbosity', 'quiet') "restore-$i.txt" | Out-Null
        Invoke-Dotnet @('build', $project, '--no-restore', '--configuration', 'Release', '--verbosity', 'quiet') "build-$i.txt" | Out-Null
    }
    $vulnerabilities = 0
    $packageUnion = @{}
    for ($i = 0; $i -lt $projects.Count; $i++) {
        $auditText = Invoke-Dotnet @('list', $projects[$i], 'package', '--vulnerable', '--include-transitive', '--format', 'json', '--no-restore') "nuget-audit-$i.json"
        $audit = $auditText | ConvertFrom-Json
        # Successful CLI execution is insufficient if JSON contains lookup problems or vulnerable rows.
        if ($audit.PSObject.Properties['problems'] -and @($audit.problems).Count) { throw 'NuGet audit reported problems.' }
        foreach ($project in $audit.projects) {
            if ($project.PSObject.Properties['frameworks']) {
                foreach ($framework in $project.frameworks) {
                    foreach ($property in @('topLevelPackages', 'transitivePackages')) {
                        if ($framework.PSObject.Properties[$property]) { $vulnerabilities += @($framework.$property).Count }
                    }
                }
            }
        }
        $lock = Get-Content -LiteralPath (Join-Path (Split-Path $projects[$i]) 'packages.lock.json') -Raw | ConvertFrom-Json
        foreach ($framework in $lock.dependencies.PSObject.Properties) {
            foreach ($package in $framework.Value.PSObject.Properties) {
                $identity = $package.Name.ToLowerInvariant() + '/' + $package.Value.resolved
                if ($packageUnion.ContainsKey($identity) -and $packageUnion[$identity].Value.contentHash -cne $package.Value.contentHash) {
                    throw "Conflicting locked content hashes: $identity"
                }
                $packageUnion[$identity] = $package
            }
        }
    }
    if ($vulnerabilities -ne 0) { throw 'NuGet audit reported vulnerable packages.' }
    $licenseInventory = @()
    foreach ($identity in ($packageUnion.Keys | Sort-Object)) {
            $package = $packageUnion[$identity]
            $id = $package.Name.ToLowerInvariant(); $resolved = $package.Value.resolved
            $packageRoot = Join-Path $packages "$id/$resolved"
            $archive = Join-Path $packageRoot "$id.$resolved.nupkg"
            # NuGet's content hash excludes package signing data; raw ZIP SHA512 is a different scope.
            $signature = Invoke-Dotnet @('nuget', 'verify', $archive, '--all', '--verbosity', 'normal') "signature-$id-$resolved.txt"
            $verifiedHash = [regex]::Match($signature, '(?m)^Content hash:\s*(\S+)').Groups[1].Value
            if ($verifiedHash -cne $package.Value.contentHash) { throw "Verified package content hash mismatch: $id" }
            [xml]$spec = Get-Content -LiteralPath (Join-Path $packageRoot "$id.nuspec") -Raw
            $license = $spec.package.metadata.SelectSingleNode("*[local-name()='license']")
            $notice = Join-Path $packageRoot 'LICENSES.txt'
            $licenseInventory += [pscustomobject]@{ package = $package.Name; version = $resolved
                license = $(if ($null -ne $license) { $license.InnerText } else { 'Bundled multi-license notices' })
                noticeSha256 = $(if (Test-Path -LiteralPath $notice) { (Get-FileHash -LiteralPath $notice -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null }) }
    }
    $licenseInventory | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'package-licenses.json') -Encoding utf8
    $tools = Join-Path $packages 'grpc.tools/2.84.0/tools/windows_x64'
    foreach ($pair in @(@('protoc.exe', $pins.protobufTools.protocSha256), @('grpc_csharp_plugin.exe', $pins.protobufTools.csharpPluginSha256))) {
        if ((Get-FileHash -LiteralPath (Join-Path $tools $pair[0]) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pair[1]) { throw 'Protobuf tool hash mismatch.' }
    }
    $compilerVersion = (& (Join-Path $tools 'protoc.exe') --version).Trim()
    if ($LASTEXITCODE -ne 0 -or $compilerVersion -ne $pins.protobufTools.protocVersion) { throw 'Compiler version mismatch.' }
    $probe = Invoke-Dotnet @('Shared/DependencyProbe/bin/Release/net10.0/DependencyProbe.dll') 'native-probe.json'
    $summary = [pscustomobject]@{ sdk = $version; projects = $projects.Count; lockedPackages = $licenseInventory.Count
        vulnerablePackagesReported = $vulnerabilities; compiler = $compilerVersion; nativeProbe = ($probe | ConvertFrom-Json) }
    $summary | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $EvidenceRoot 'summary.json') -Encoding utf8
    $summary | ConvertTo-Json -Depth 5 -Compress
}
finally { Pop-Location }
