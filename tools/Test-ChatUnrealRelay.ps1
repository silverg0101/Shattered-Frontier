[CmdletBinding()]
param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8', [ValidateRange(60,300)][int]$TimeoutSeconds = 150)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project = Join-Path $repo 'GameClient\ShatteredFrontier.uproject'
$dotnet = 'C:/Projects/Shattered-Frontier/Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe'
foreach ($path in @($editor,$project,$dotnet)) { if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing input: $path" } }
$root = Join-Path $repo ('Artifacts\ChatUnrealRelay\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$null = New-Item -ItemType Directory -Path $root
$listener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback,0)
$listener.Start(); $port = $listener.LocalEndpoint.Port; $listener.Stop()
$owned = [Collections.Generic.List[Diagnostics.Process]]::new()
function Launch([string]$file,[string[]]$arguments,[string]$name) {
    $process = Start-Process -FilePath $file -ArgumentList $arguments -PassThru -WindowStyle Hidden `
        -RedirectStandardOutput (Join-Path $root "$name.out") -RedirectStandardError (Join-Path $root "$name.err")
    $null = $process.Handle
    $owned.Add($process)
    return $process
}
function Wait-Marker([string]$path,[string]$pattern) {
    $until = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $until) {
        if ((Test-Path -LiteralPath $path) -and (Select-String -LiteralPath $path -Pattern $pattern -Quiet)) { return }
        foreach ($process in $owned) { if ($process.HasExited -and $process.ExitCode -ne 0) { throw "Process $($process.Id) failed; inspect $root" } }
        Start-Sleep -Milliseconds 300
    }
    throw "Timed out waiting for $pattern in $path"
}
try {
    $fixture = Join-Path $repo 'BackendServices/Chat.Host.Tests/Chat.Host.Tests.csproj'
    & $dotnet restore $fixture --locked-mode --verbosity quiet
    if ($LASTEXITCODE -ne 0) { throw 'Chat fixture restore failed.' }
    & $dotnet build $fixture --no-restore --configuration Release --verbosity quiet
    if ($LASTEXITCODE -ne 0) { throw 'Chat fixture build failed.' }
    $dll = Join-Path $repo 'BackendServices/Chat.Host.Tests/bin/Release/net10.0/Chat.Host.Tests.dll'
    $hostProcess = Launch $dotnet @(('"'+$dll+'"'),'unreal-relay',"$port") 'relay'
    Wait-Marker (Join-Path $root 'relay.out') 'CHAT_UNREAL_RELAY_READY'
    foreach ($role in @('A','B')) {
        $log = Join-Path $root "$role.log"
        $user = Join-Path $root "$role-user"
        $null = New-Item -ItemType Directory -Path $user
        $args = @(('"'+$project+'"'),'-game','-unattended','-NullRHI','-nosound','-nop4',
            ('-UserDir="'+$user+'"'),('-abslog="'+$log+'"'),('-ChatRelayRole='+$role),
            ('-ChatRelayEndpoint=ws://127.0.0.1:'+ $port +'/v1/chat'),('-ChatRelayRoot="'+$root+'"'),
            '-ExecCmds="Automation RunTests ShatteredFrontier.Chat.TwoClientRelay"','-TestExit="Automation Test Queue Empty"')
        [void](Launch $editor $args $role)
    }
    Wait-Marker (Join-Path $root 'A.log') 'CHAT_UNREAL_CLIENT_PASS A'
    Wait-Marker (Join-Path $root 'B.log') 'CHAT_UNREAL_CLIENT_PASS B'
    foreach ($role in @('A','B')) {
        $log = Join-Path $root "$role.log"
        if (-not (Select-String -LiteralPath $log -Pattern 'Test Completed. Result=\{Success\}' -Quiet)) {
            Wait-Marker $log 'Test Completed. Result=\{Success\}'
        }
        if (Select-String -LiteralPath $log -Pattern 'Test Completed. Result=\{Fail|CHAT_UNREAL_CLIENT_FAILED' -Quiet) { throw "$role automation failed." }
    }
    Write-Output "PASS Chat Unreal relay: 2 clients, party round trip, not_delivered rejection. Evidence: $root"
}
finally {
    foreach ($process in $owned) { if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force; $process.WaitForExit() } }
    & $dotnet build-server shutdown | Out-Null
}
