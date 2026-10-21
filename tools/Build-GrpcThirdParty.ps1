<#
.SYNOPSIS
Builds the pinned gRPC C++ release (with protobuf, abseil, re2, c-ares) from source for Win64 and
generates the C++ stubs for proto/ into the server-only ShatteredFrontierBackend module.

.DESCRIPTION
Pins live in GameClient/Source/ThirdParty/gRPC/gRPC.lock.json. Sources, tools and binaries stay in the
git-ignored Artifacts/ThirdParty/gRPC. TLS and zlib link the engine's own libraries so one copy of each
exists per process; the TLS choice is isolated in Get-TlsCMakeArgs here and in gRPC.Build.cs.
Run from any directory; the editor may stay open. A full build takes roughly 20-40 minutes.

.EXAMPLE
powershell -File tools/Build-GrpcThirdParty.ps1
powershell -File tools/Build-GrpcThirdParty.ps1 -StubsOnly
powershell -File tools/Build-GrpcThirdParty.ps1 -Platform Linux -EngineRoot C:\UnrealSource\UE_5.8.3 -LinuxMultiArchRoot C:\UnrealToolchains\v26_clang-20.1.8-rockylinux8
#>
[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateSet('Win64','Linux')][string]$Platform = 'Win64',
    # Linux only: the v26_clang-20.1.8-rockylinux8 SDK root; -EngineRoot must then be the source engine (it has the Unix zlib).
    [string]$LinuxMultiArchRoot = $env:LINUX_MULTIARCH_ROOT,
    [switch]$StubsOnly
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$lock = Get-Content -Raw (Join-Path $repoRoot 'GameClient\Source\ThirdParty\gRPC\gRPC.lock.json') | ConvertFrom-Json
$work = Join-Path $repoRoot 'Artifacts\ThirdParty\gRPC'
$src = Join-Path $work 'src'
$install = Join-Path $work "install\$Platform"
$engineThirdParty = Join-Path $EngineRoot 'Engine\Source\ThirdParty'

function Invoke-Checked([string]$exe, [string[]]$argv) {
    & $exe @argv
    if ($LASTEXITCODE -ne 0) { throw "$exe failed with exit code $LASTEXITCODE" }
}

function Get-TlsCMakeArgs {
    # Only provider today: the engine's OpenSSL, which the engine's own SSL/HTTP modules already link.
    # Switching provider means changing this function, the lock's "tls" entry and gRPC.Build.cs together.
    if ($lock.tls.provider -ne 'EngineOpenSSL') { throw "Unsupported TLS provider $($lock.tls.provider)" }
    $root = Join-Path $engineThirdParty "OpenSSL\$($lock.tls.version)"
    $lib = Join-Path $root 'lib\Win64\VS2015\Release'
    return @('-DgRPC_SSL_PROVIDER=package', '-DOPENSSL_USE_STATIC_LIBS=TRUE',
        "-DOPENSSL_INCLUDE_DIR=$(Join-Path $root 'include\Win64\VS2015')",
        "-DOPENSSL_SSL_LIBRARY=$(Join-Path $lib 'libssl.lib')",
        "-DOPENSSL_CRYPTO_LIBRARY=$(Join-Path $lib 'libcrypto.lib')")
}

function Sync-Source {
    if (-not (Test-Path (Join-Path $src '.git'))) {
        Invoke-Checked git @('clone', '-q', '--depth', '1', '--branch', $lock.grpc.tag, '-c', 'advice.detachedHead=false', $lock.grpc.repository, $src)
    }
    $head = (& git -C $src rev-parse HEAD).Trim()
    if ($head -ne $lock.grpc.commit) { throw "gRPC checkout is $head, lock wants $($lock.grpc.commit)" }
    foreach ($p in $lock.submodules.PSObject.Properties) {
        Invoke-Checked git @('-C', $src, 'submodule', 'update', '--init', '--depth', '1', '-q', $p.Name)
        $actual = (& git -C (Join-Path $src $p.Name) rev-parse HEAD).Trim()
        if ($actual -ne $p.Value) { throw "$($p.Name) is $actual, lock wants $($p.Value)" }
    }
}

function Get-Tools {
    $venv = Join-Path $work 'toolenv'
    $wheels = Join-Path $work 'wheels'
    $py = Join-Path $venv 'Scripts\python.exe'
    if (-not (Test-Path $py)) { Invoke-Checked py @('-3.12', '-m', 'venv', $venv) }
    $names = @($lock.tools.PSObject.Properties | ForEach-Object { $_.Name })
    $missing = @($names | Where-Object { -not (Test-Path (Join-Path $wheels $_)) })
    if ($missing.Count -gt 0) {
        $specs = $missing | ForEach-Object { ($_ -split '-')[0] + '==' + ($_ -split '-')[1] }
        Invoke-Checked $py (@('-m', 'pip', 'download', '-q', '--only-binary=:all:', '--no-deps', '-d', $wheels) + $specs)
    }
    foreach ($p in $lock.tools.PSObject.Properties) {
        $hash = (Get-FileHash -Algorithm SHA256 (Join-Path $wheels $p.Name)).Hash.ToLowerInvariant()
        if ($hash -ne $p.Value) { throw "$($p.Name) hash $hash does not match lock" }
    }
    Invoke-Checked $py (@('-m', 'pip', 'install', '-q', '--no-index', '--no-deps') + ($names | ForEach-Object { Join-Path $wheels $_ }))
    return (Join-Path $venv 'Scripts')
}

function Build-Grpc([string]$toolDir) {
    # Must be the MSVC toolset UBT selects (see "Using Visual Studio ... toolchain" in a UBT log); a newer
    # STL leaves unresolved __std_* vector-algorithm symbols at the UE link.
    $vcvars = @(& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -products * -version $lock.msvc.vsVersionRange -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'VC\Auxiliary\Build\vcvars64.bat') | Select-Object -First 1
    if (-not $vcvars) { throw "vcvars64.bat for Visual Studio $($lock.msvc.vsVersionRange) not found; install the MSVC $($lock.msvc.toolset) toolset UE uses." }
    $zlib = Join-Path $engineThirdParty "zlib\$($lock.zlib.version)"
    $build = Join-Path $work 'build\Win64'
    $cmakeArgs = @('-S', $src, '-B', $build, '-G', 'Ninja',
        "-DCMAKE_MAKE_PROGRAM=$(Join-Path $toolDir 'ninja.exe')",
        '-DCMAKE_BUILD_TYPE=Release', "-DCMAKE_INSTALL_PREFIX=$install",
        '-DCMAKE_POLICY_DEFAULT_CMP0091=NEW', '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL',
        '-DCMAKE_CXX_STANDARD=17', '-DABSL_PROPAGATE_CXX_STD=ON', '-DBUILD_SHARED_LIBS=OFF',
        '-DCMAKE_C_FLAGS=/Brepro', '-DCMAKE_CXX_FLAGS=/Brepro /EHsc', '-DCMAKE_STATIC_LINKER_FLAGS=/Brepro',
        '-DgRPC_INSTALL=ON', '-DgRPC_BUILD_TESTS=OFF', '-DgRPC_DOWNLOAD_ARCHIVES=OFF',
        '-DgRPC_ABSL_PROVIDER=module', '-DgRPC_PROTOBUF_PROVIDER=module', '-DgRPC_RE2_PROVIDER=module', '-DgRPC_CARES_PROVIDER=module',
        '-DgRPC_ZLIB_PROVIDER=package', "-DZLIB_INCLUDE_DIR=$(Join-Path $zlib 'include')",
        "-DZLIB_LIBRARY=$(Join-Path $zlib 'lib\Win64\Release\zlibstatic.lib')",
        '-Dprotobuf_BUILD_TESTS=OFF', '-Dprotobuf_WITH_ZLIB=OFF', '-Dprotobuf_MSVC_STATIC_RUNTIME=OFF', '-DCARES_MSVC_STATIC_RUNTIME=OFF',
        '-DgRPC_BUILD_GRPC_CSHARP_PLUGIN=OFF', '-DgRPC_BUILD_GRPC_NODE_PLUGIN=OFF', '-DgRPC_BUILD_GRPC_OBJECTIVE_C_PLUGIN=OFF',
        '-DgRPC_BUILD_GRPC_PHP_PLUGIN=OFF', '-DgRPC_BUILD_GRPC_PYTHON_PLUGIN=OFF', '-DgRPC_BUILD_GRPC_RUBY_PLUGIN=OFF',
        '-DgRPC_BUILD_CSHARP_EXT=OFF') + (Get-TlsCMakeArgs)
    $cmake = Join-Path $toolDir 'cmake.exe'
    $quoted = ($cmakeArgs | ForEach-Object { '"' + $_ + '"' }) -join ' '
    # A batch file avoids PowerShell 5.1 native-argument quoting; vcvars must run in the same cmd process.
    New-Item -ItemType Directory -Force $build | Out-Null
    $batch = Join-Path $build 'build-grpc.cmd'
    Set-Content -Encoding ascii $batch @(
        '@echo off', "call `"$vcvars`" -vcvars_ver=$($lock.msvc.toolset) >nul || exit /b 1",
        "`"$cmake`" $quoted || exit /b 1",
        "`"$cmake`" --build `"$build`" --parallel || exit /b 1",
        "`"$cmake`" --install `"$build`" || exit /b 1")
    Invoke-Checked cmd.exe @('/d', '/c', $batch)

    $manifest = [ordered]@{ grpcCommit = $lock.grpc.commit; tls = $lock.tls; libraries = [ordered]@{} }
    Get-ChildItem (Join-Path $install 'lib') -Filter *.lib | Sort-Object Name | ForEach-Object {
        $manifest.libraries[$_.Name] = (Get-FileHash -Algorithm SHA256 $_.FullName).Hash.ToLowerInvariant()
    }
    $manifest | ConvertTo-Json -Depth 4 | Set-Content -Encoding utf8 (Join-Path $install 'manifest.json')
}


function Build-GrpcLinux([string]$toolDir) {
    # INT-03 0b, Windows cross route: same two-pass prefixed-BoringSSL build as tools/build-grpc-linux.sh,
    # with UE's v26 Windows-hosted Linux SDK through tools/grpc-linux-toolchain.cmake.
    if (-not $LinuxMultiArchRoot) { throw 'Pass -LinuxMultiArchRoot (the v26_clang-20.1.8-rockylinux8 directory).' }
    $env:SF_LINUX_SDK_ROOT = Join-Path $LinuxMultiArchRoot $lock.linuxToolchain.triple
    foreach ($p in $lock.linuxSubmodules.PSObject.Properties) {
        Invoke-Checked git @('-C', $src, 'submodule', 'update', '--init', '--depth', '1', '-q', $p.Name)
        if ((& git -C (Join-Path $src $p.Name) rev-parse HEAD).Trim() -ne $p.Value) { throw "$($p.Name) does not match the lock" }
    }
    $go = Join-Path $work 'tools\go\bin\go.exe'
    if (-not (Test-Path $go)) {
        $zip = Join-Path $work 'dl\go-windows.zip'
        New-Item -ItemType Directory -Force (Split-Path $zip) | Out-Null
        if (-not (Test-Path $zip)) { Invoke-Checked curl.exe @('-fsSL', '--retry', '3', '-o', $zip, $lock.windowsCrossTools.go.url) }
        if ((Get-FileHash -Algorithm SHA256 $zip).Hash.ToLowerInvariant() -ne $lock.windowsCrossTools.go.sha256) { throw 'Go archive hash does not match the lock' }
        Expand-Archive -Force $zip (Join-Path $work 'tools')
    }
    $env:GOCACHE = Join-Path $work 'gocache'; $env:GOPATH = Join-Path $work 'gopath'; $env:GOTOOLCHAIN = 'local'; $env:GOFLAGS = '-mod=mod'
    $zlib = Join-Path $engineThirdParty "zlib\$($lock.zlib.version)"
    $prefix = $lock.linuxTls.prefix
    $cmake = Join-Path $toolDir 'cmake.exe'
    $common = @('-S', $src, '-G', 'Ninja', "-DCMAKE_MAKE_PROGRAM=$(Join-Path $toolDir 'ninja.exe')",
        "-DCMAKE_TOOLCHAIN_FILE=$(Join-Path $repoRoot 'tools\grpc-linux-toolchain.cmake')", '-DCMAKE_BUILD_TYPE=Release',
        '-DCMAKE_CXX_STANDARD=17', '-DABSL_PROPAGATE_CXX_STD=ON', '-DBUILD_SHARED_LIBS=OFF',
        '-DgRPC_INSTALL=ON', '-DgRPC_BUILD_TESTS=OFF', '-DgRPC_DOWNLOAD_ARCHIVES=OFF', '-DgRPC_BUILD_CODEGEN=OFF',
        '-DBUILD_TESTING=OFF', '-DRE2_BUILD_TESTING=OFF', '-DCARES_BUILD_TESTS=OFF', '-Dutf8_range_ENABLE_TESTS=OFF',
        '-Dprotobuf_BUILD_PROTOC_BINARIES=OFF', '-Dprotobuf_BUILD_TESTS=OFF', '-Dprotobuf_WITH_ZLIB=OFF',
        '-DgRPC_ABSL_PROVIDER=module', '-DgRPC_PROTOBUF_PROVIDER=module', '-DgRPC_RE2_PROVIDER=module', '-DgRPC_CARES_PROVIDER=module',
        '-DgRPC_SSL_PROVIDER=module', '-DgRPC_ZLIB_PROVIDER=package', "-DZLIB_INCLUDE_DIR=$(Join-Path $zlib 'include')",
        "-DZLIB_LIBRARY=$(Join-Path $zlib "lib\Unix\$($lock.linuxToolchain.triple)\Release\libz.a")")
    $pass1 = Join-Path $work 'build\Linux-pass1'
    Invoke-Checked $cmake ($common + @('-B', $pass1))
    Invoke-Checked $cmake @('--build', $pass1, '--target', 'ssl', 'crypto')
    $symbols = Join-Path $work 'build\boringssl-symbols.txt'; $prefixInclude = Join-Path $work 'build\prefix-include'
    $bssl = Join-Path $pass1 'third_party\boringssl-with-bazel'
    Push-Location (Join-Path $src 'third_party\boringssl-with-bazel\util')
    try {
        Invoke-Checked $go @('run', 'read_symbols.go', '-obj-file-format', 'elf', '-out', $symbols, (Join-Path $bssl 'libcrypto.a'), (Join-Path $bssl 'libssl.a'))
        # gRPC's own TLS code includes <openssl/*.h>, so every target needs the prefix headers.
        New-Item -ItemType Directory -Force $prefixInclude | Out-Null
        Invoke-Checked $go @('run', 'make_prefix_headers.go', '-out', $prefixInclude, $symbols)
    } finally { Pop-Location }
    $build = Join-Path $work 'build\Linux'
    if (Test-Path $build) { Remove-Item -Recurse -Force $build }
    if (Test-Path $install) { Remove-Item -Recurse -Force $install }
    $env:SF_LINUX_EXTRA_FLAGS = "-DBORINGSSL_PREFIX=$prefix -I$($prefixInclude.Replace('\', '/'))"
    Invoke-Checked $cmake ($common + @('-B', $build, "-DCMAKE_INSTALL_PREFIX=$install", "-DBORINGSSL_PREFIX=$prefix",
        "-DBORINGSSL_PREFIX_SYMBOLS=$symbols", "-DGO_EXECUTABLE=$go"))
    Invoke-Checked $cmake @('--build', $build)
    Invoke-Checked $cmake @('--install', $build)
    # Fail closed if any OpenSSL-named symbol escaped the prefix.
    $nm = Join-Path $env:SF_LINUX_SDK_ROOT 'bin\x86_64-unknown-linux-gnu-nm.exe'
    $leaks = & $nm -g --defined-only (Join-Path $install 'lib\libssl.a') (Join-Path $install 'lib\libcrypto.a') |
        Where-Object { $_ -match '^\S*\s+[TDBR]\s+(SSL|EVP|X509|BIO|ERR|CRYPTO)_' }
    if ($leaks) { throw "Unprefixed BoringSSL symbols in the install, e.g. $($leaks | Select-Object -First 1)" }
    # UE links libc++ (std::__1); any libstdc++ reference means an object missed the toolchain's flags.
    $stdcxx = & $nm -u (Get-ChildItem (Join-Path $install 'lib') -Filter *.a).FullName 2>$null | Where-Object { $_ -match '_ZNSt7__cxx11|_ZSt\d+__throw_|_ZNSo|__once_proxy' }
    if ($stdcxx) { throw "libstdc++ references in the install, e.g. $($stdcxx | Select-Object -First 1)" }
    $manifest = [ordered]@{ grpcCommit = $lock.grpc.commit; tls = "PrefixedBoringSSL:$prefix"; libraries = [ordered]@{} }
    Get-ChildItem (Join-Path $install 'lib') -Filter *.a | Sort-Object Name | ForEach-Object {
        $manifest.libraries[$_.Name] = (Get-FileHash -Algorithm SHA256 $_.FullName).Hash.ToLowerInvariant()
    }
    $manifest | ConvertTo-Json -Depth 4 | Set-Content -Encoding utf8 (Join-Path $install 'manifest.json')
    Write-Host "SF_GRPC_LINUX_DONE Install=$install"
}

function Update-Stubs {
    $bin = Join-Path $install 'bin'
    $out = Join-Path $repoRoot 'GameClient\Source\ShatteredFrontierBackend\Private\Generated'
    $protoRoot = Join-Path $repoRoot 'proto'
    New-Item -ItemType Directory -Force $out | Out-Null
    $protos = Get-ChildItem -Recurse $protoRoot -Filter *.proto | ForEach-Object { $_.FullName.Substring($protoRoot.Length + 1) -replace '\\', '/' }
    Invoke-Checked (Join-Path $bin 'protoc.exe') (@("--proto_path=$protoRoot", "--cpp_out=$out", "--grpc_out=$out",
        "--plugin=protoc-gen-grpc=$(Join-Path $bin 'grpc_cpp_plugin.exe')") + $protos)
    # UBT would compile .cc files with UE warnings and macros; SFLedgerProto.cpp includes them inside the third-party guards instead.
    Get-ChildItem -Recurse $out -Filter *.cc | ForEach-Object { Move-Item -Force $_.FullName ($_.FullName + '.inc') }
    Write-Host "Generated stubs for $($protos -join ', ') in $out"
}

if ($Platform -eq 'Linux') { Sync-Source; Build-GrpcLinux (Get-Tools); return }
if (-not $StubsOnly) {
    Sync-Source
    Build-Grpc (Get-Tools)
}
Update-Stubs
