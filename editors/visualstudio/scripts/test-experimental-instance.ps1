#requires -Version 7.0

[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [string]$RootSuffix = 'RLSPhase4',

    [int]$TimeoutSeconds = 45
)

$ErrorActionPreference = 'Stop'

$visualStudioDirectory = Split-Path -Parent $PSScriptRoot
$repositoryDirectory = (Resolve-Path (Join-Path $visualStudioDirectory '..\..')).Path
$solutionPath = Join-Path $visualStudioDirectory 'RandoLogicScript.VisualStudio.sln'
$vsixPath = Join-Path $visualStudioDirectory "src\bin\$Configuration\net472\RandoLogicScript.VisualStudio.vsix"
$fixtureSource = Join-Path $repositoryDirectory 'editors\vscode\test-fixture'
$traceDirectory = Join-Path $env:TEMP 'VisualStudio\LSP'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'

if ($null -eq ('RlsVisualStudioRotV2' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class RlsVisualStudioRotV2
{
    [DllImport("ole32.dll", CharSet = CharSet.Unicode)]
    private static extern int CLSIDFromProgID(string progId, out Guid clsid);

    [DllImport("oleaut32.dll")]
    private static extern int GetActiveObject(
        ref Guid clsid,
        IntPtr reserved,
        [MarshalAs(UnmanagedType.IUnknown)] out object value);

    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);

    public static object Get(string progId)
    {
        Guid clsid;
        int result = CLSIDFromProgID(progId, out clsid);
        if (result < 0) Marshal.ThrowExceptionForHR(result);
        object value;
        result = GetActiveObject(ref clsid, IntPtr.Zero, out value);
        if (result < 0) Marshal.ThrowExceptionForHR(result);
        return value;
    }

    public static int GetProcessId(object dteObject)
    {
        dynamic dte = dteObject;
        uint processId;
        GetWindowThreadProcessId(new IntPtr(dte.MainWindow.HWnd), out processId);
        return unchecked((int)processId);
    }
}
'@
}

function Wait-Until {
    param(
        [scriptblock]$Condition,
        [string]$Description
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $result = & $Condition
        if ($result) {
            return $result
        }
        [System.Threading.ManualResetEventSlim]::new($false).Wait(250)
    } while ([DateTime]::UtcNow -lt $deadline)

    throw "Timed out waiting for $Description."
}

function Get-ExperimentalProcesses {
    Get-CimInstance Win32_Process | Where-Object {
        $_.Name -eq 'devenv.exe' -and $_.CommandLine -match "(?i)(?:/rootSuffix|/rootsuffix)\s+$([regex]::Escape($RootSuffix))\b"
    }
}

function Stop-ExperimentalProcesses {
    Get-ExperimentalProcesses | ForEach-Object {
        Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue
    }
}

function Get-OwnedServers {
    $hostIds = @(Get-ExperimentalProcesses | ForEach-Object ProcessId)
    if ($hostIds.Count -eq 0) {
        return @()
    }
    return @(Get-CimInstance Win32_Process | Where-Object {
        $_.Name -eq 'rls_language_server.exe' -and $_.ParentProcessId -in $hostIds
    })
}

function Start-ExperimentalHost {
    param(
        [string]$ActivityLog
    )

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $devenvPath
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.ArgumentList.Add('/RootSuffix')
    $startInfo.ArgumentList.Add($RootSuffix)
    $startInfo.ArgumentList.Add('/Log')
    $startInfo.ArgumentList.Add($ActivityLog)
    $startInfo.EnvironmentVariables.Remove('RLS_LANGUAGE_SERVER_PATH')
    return [System.Diagnostics.Process]::Start($startInfo)
}

function Get-ExperimentalDte {
    param([int]$ProcessId)

    $result = Wait-Until {
        try {
            $dte = [RlsVisualStudioRotV2]::Get('VisualStudio.DTE.18.0')
            if ([RlsVisualStudioRotV2]::GetProcessId($dte) -eq $ProcessId) {
                return [pscustomobject]@{ Dte = $dte }
            }
        }
        catch [System.Runtime.InteropServices.COMException] {
        }
        return $null
    } "Visual Studio DTE for process $ProcessId"
    return $result
}

function Open-HostTarget {
    param(
        $Dte,
        [string]$Command,
        [string]$Path
    )

    $Dte.ExecuteCommand($Command, "`"$Path`"")
}

function Get-NewTrace {
    param([DateTime]$Since)

    Wait-Until {
        Get-ChildItem $traceDirectory -Filter 'RlsLanguageClient-*.log' -ErrorAction SilentlyContinue |
            Where-Object LastWriteTimeUtc -ge $Since |
            Sort-Object LastWriteTimeUtc -Descending |
            Select-Object -First 1
    } 'a new RLS LSP trace'
}

function Wait-TraceText {
    param(
        [string]$TracePath,
        [string]$Pattern,
        [string]$Description
    )

    Wait-Until {
        if ((Test-Path -LiteralPath $TracePath) -and
            (Select-String -LiteralPath $TracePath -Pattern $Pattern -Quiet)) {
            return $true
        }
        return $false
    } $Description | Out-Null
}

function Assert-TraceContains {
    param(
        [string]$TracePath,
        [string]$Pattern,
        [string]$Description
    )

    if (-not (Select-String -LiteralPath $TracePath -Pattern $Pattern -Quiet)) {
        throw "Trace did not contain $Description ($Pattern)."
    }
}

if (-not (Test-Path -LiteralPath $vswhere)) {
    throw 'Visual Studio Installer vswhere.exe was not found.'
}

$instancesJson = (& $vswhere -products * -format json) -join [Environment]::NewLine
$instances = @($instancesJson | ConvertFrom-Json)
$instance = $instances | Sort-Object installationVersion -Descending | Select-Object -First 1
if ($null -eq $instance) {
    throw 'A complete Visual Studio installation was not found.'
}

$devenvPath = Join-Path $instance.installationPath 'Common7\IDE\devenv.exe'
$msbuildPath = Join-Path $instance.installationPath 'MSBuild\Current\Bin\amd64\MSBuild.exe'
if (-not (Test-Path -LiteralPath $devenvPath) -or -not (Test-Path -LiteralPath $msbuildPath)) {
    throw "Visual Studio instance '$($instance.installationPath)' is incomplete."
}

$temporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) "RLS Phase 4 Ω $([guid]::NewGuid().ToString('N'))"
$workspaceFixture = Join-Path $temporaryRoot 'workspace'
$standaloneFixture = Join-Path $temporaryRoot 'standalone files'

try {
    Stop-ExperimentalProcesses

    & $msbuildPath $solutionPath /t:Build /p:Configuration=$Configuration /m /v:minimal
    if ($LASTEXITCODE -ne 0) {
        throw "Visual Studio extension build failed with exit code $LASTEXITCODE."
    }
    & (Join-Path $PSScriptRoot 'validate-vsix.ps1') -VsixPath $vsixPath

    $deploymentArguments = @(
        $solutionPath,
        '/t:Build',
        "/p:Configuration=$Configuration",
        '/p:DeployExtension=true',
        "/p:DeployTargetInstanceId=$($instance.instanceId)",
        "/p:VSSDKTargetPlatformRegRootSuffix=$RootSuffix",
        '/m',
        '/v:minimal'
    )
    & $msbuildPath @deploymentArguments
    if ($LASTEXITCODE -ne 0) {
        throw "Experimental extension deployment failed with exit code $LASTEXITCODE."
    }

    New-Item -ItemType Directory -Path $workspaceFixture, $standaloneFixture -Force | Out-Null
    Copy-Item (Join-Path $fixtureSource 'diagnostic.rls') $workspaceFixture
    Copy-Item (Join-Path $fixtureSource 'rls.json') $workspaceFixture
    New-Item -ItemType Directory -Path (Join-Path $workspaceFixture '.vs') -Force | Out-Null
    @{
        'randoLogicScript.trace.server' = 'Verbose'
    } | ConvertTo-Json | Set-Content (Join-Path $workspaceFixture '.vs\VSWorkspaceSettings.json') -Encoding UTF8

    $workspaceStart = [DateTime]::UtcNow
    $workspaceActivityLog = Join-Path $temporaryRoot 'workspace-activity.xml'
    $workspaceSource = Join-Path $workspaceFixture 'diagnostic.rls'
    $workspaceHost = Start-ExperimentalHost $workspaceActivityLog
    $workspaceDte = (Get-ExperimentalDte $workspaceHost.Id).Dte
    $null = ($workspaceDte.MainWindow.Visible = $true)
    Open-HostTarget $workspaceDte 'File.OpenFolder' $workspaceFixture
    Open-HostTarget $workspaceDte 'File.OpenFile' $workspaceSource

    $server = Wait-Until {
        $servers = @(Get-OwnedServers)
        if ($servers.Count -eq 1) { return $servers[0] }
        return $null
    } 'one bundled language-server process'
    if ($server.CommandLine -notmatch [regex]::Escape("$RootSuffix\Extensions")) {
        throw "Visual Studio did not launch the bundled server: $($server.CommandLine)"
    }

    $workspaceTrace = Get-NewTrace $workspaceStart
    Wait-TraceText $workspaceTrace.FullName '"method"\s*:\s*"textDocument/didOpen"' 'didOpen'
    Wait-TraceText $workspaceTrace.FullName '"code"\s*:\s*"RLS-T006"' 'RLS-T006 diagnostics'
    Wait-TraceText $workspaceTrace.FullName '"method"\s*:\s*"textDocument/semanticTokens/full"' 'semantic tokens'
    Wait-TraceText $workspaceTrace.FullName '"method"\s*:\s*"textDocument/documentSymbol"' 'document symbols'
    Assert-TraceContains $workspaceTrace.FullName '"documentChanges"\s*:\s*true' 'workspace edit documentChanges support'
    Assert-TraceContains $workspaceTrace.FullName '"hover"\s*:\s*\{' 'hover capability'
    Assert-TraceContains $workspaceTrace.FullName '"signatureHelp"\s*:\s*\{' 'signature help capability'
    Assert-TraceContains $workspaceTrace.FullName '"references"\s*:\s*\{' 'references capability'
    Assert-TraceContains $workspaceTrace.FullName '"rename"\s*:\s*\{' 'rename capability'
    Assert-TraceContains $workspaceTrace.FullName '"rootUri"\s*:\s*"file:///.*?/workspace"' 'encoded Open Folder rootUri'

    $manifestPath = Join-Path $workspaceFixture 'rls.json'
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    $manifest | Add-Member -NotePropertyName exclude -NotePropertyValue @('ignored.rls') -Force
    $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
    Wait-TraceText $workspaceTrace.FullName '"method"\s*:\s*"workspace/didChangeWatchedFiles"' 'watched manifest notification'

    $oldServerId = $server.ProcessId
    Stop-Process -Id $oldServerId -Force
    $replacement = Wait-Until {
        $servers = @(Get-OwnedServers | Where-Object ProcessId -ne $oldServerId)
        if ($servers.Count -eq 1) { return $servers[0] }
        return $null
    } 'exactly one replacement server'
    if (@(Get-OwnedServers).Count -ne 1) {
        throw 'Visual Studio recovery left duplicate language-server processes.'
    }

    Stop-ExperimentalProcesses
    Wait-Until { if (@(Get-OwnedServers).Count -eq 0) { return $true }; return $false } 'workspace server cleanup' | Out-Null

    $standaloneSource = Join-Path $standaloneFixture 'unicode Ω logic.rls'
    'define unicode(): "😀" == missing' | Set-Content -LiteralPath $standaloneSource -Encoding UTF8
    $standaloneStart = [DateTime]::UtcNow
    $standaloneActivityLog = Join-Path $temporaryRoot 'standalone-activity.xml'
    $standaloneHost = Start-ExperimentalHost $standaloneActivityLog
    $standaloneDte = (Get-ExperimentalDte $standaloneHost.Id).Dte
    $null = ($standaloneDte.MainWindow.Visible = $true)
    Open-HostTarget $standaloneDte 'File.OpenFile' $standaloneSource
    Wait-Until { if (@(Get-OwnedServers).Count -eq 1) { return $true }; return $false } 'standalone server activation' | Out-Null
    $standaloneTrace = Get-NewTrace $standaloneStart
    Wait-TraceText $standaloneTrace.FullName '"method"\s*:\s*"textDocument/didOpen"' 'standalone didOpen'
    Wait-TraceText $standaloneTrace.FullName '"code"\s*:\s*"RLS-T006"' 'standalone UTF-16 diagnostic'
    Assert-TraceContains $standaloneTrace.FullName '"rootUri"\s*:\s*null' 'standalone null rootUri'
    Assert-TraceContains $standaloneTrace.FullName 'unicode%20%CE%A9%20logic\.rls' 'escaped Unicode/spaced file URI'

    Write-Output "Visual Studio experimental host validation passed for $($instance.displayName) $($instance.installationVersion)."
    Write-Output "Workspace trace: $($workspaceTrace.FullName)"
    Write-Output "Standalone trace: $($standaloneTrace.FullName)"
}
finally {
    Stop-ExperimentalProcesses
    if (Test-Path -LiteralPath $temporaryRoot) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}