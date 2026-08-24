#requires -Version 7.0

[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [string]$RootSuffix = 'RLSPhase4',

    [int]$TimeoutSeconds = 45,

    [switch]$SkipWhenDteUnavailable
)

$ErrorActionPreference = 'Stop'

$visualStudioDirectory = Split-Path -Parent $PSScriptRoot
$repositoryDirectory = (Resolve-Path (Join-Path $visualStudioDirectory '..\..')).Path
$solutionPath = Join-Path $visualStudioDirectory 'RandoLogicScript.VisualStudio.sln'
$vsixPath = Join-Path $visualStudioDirectory "src\bin\$Configuration\net472\RandoLogicScript.VisualStudio.vsix"
$fixtureSource = Join-Path $repositoryDirectory 'editors\vscode\test-fixture'
$traceDirectory = Join-Path $env:TEMP 'VisualStudio\LSP'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'

if ($null -eq ('RlsVisualStudioRotV4' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;

public static class RlsVisualStudioRotV4
{
    [DllImport("ole32.dll", CharSet = CharSet.Unicode)]
    private static extern int CreateBindCtx(uint reserved, out IBindCtx bindContext);

    [DllImport("ole32.dll")]
    private static extern int GetRunningObjectTable(uint reserved, out IRunningObjectTable table);

    public static object GetForProcesses(int[] processIds)
    {
        IRunningObjectTable table;
        int result = GetRunningObjectTable(0, out table);
        if (result < 0) Marshal.ThrowExceptionForHR(result);
        IBindCtx bindContext;
        result = CreateBindCtx(0, out bindContext);
        if (result < 0) Marshal.ThrowExceptionForHR(result);
        IEnumMoniker monikers;
        table.EnumRunning(out monikers);
        monikers.Reset();
        var current = new IMoniker[1];
        IntPtr fetched = Marshal.AllocCoTaskMem(sizeof(int));
        try
        {
            while (monikers.Next(1, current, fetched) == 0)
            {
                string name;
                current[0].GetDisplayName(bindContext, null, out name);
                foreach (int processId in processIds)
                {
                    if (name.StartsWith("!VisualStudio.DTE.", StringComparison.OrdinalIgnoreCase)
                        && name.EndsWith(":" + processId, StringComparison.OrdinalIgnoreCase))
                    {
                        object value;
                        table.GetObject(current[0], out value);
                        return value;
                    }
                }
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(fetched);
        }
        return null;
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
            $processIds = @($ProcessId) + @(Get-ExperimentalProcesses | ForEach-Object ProcessId)
            $dte = [RlsVisualStudioRotV4]::GetForProcesses([int[]]($processIds | Select-Object -Unique))
            if ($null -ne $dte) {
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

    & $msbuildPath $solutionPath /restore /t:Build /p:Configuration=$Configuration /m /v:minimal
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
    try {
        $workspaceDte = (Get-ExperimentalDte $workspaceHost.Id).Dte
    }
    catch {
        if ($SkipWhenDteUnavailable -and
            $_.Exception.Message -like 'Timed out waiting for Visual Studio DTE for process *') {
            Write-Warning @"
Visual Studio did not register its DTE automation object. The Experimental Instance
host assertions are skipped because this session has no interactive DTE. The harness
build, deployment, and VSIX validation completed successfully. CI runs the server
smoke and unit tests in prerequisite steps before invoking this harness.
Run this script without -SkipWhenDteUnavailable on an interactive Visual Studio 2026
machine to enforce the complete host validation.
"@
            return
        }
        throw
    }
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
    Assert-TraceContains $workspaceTrace.FullName '"method name"' 'mapped function token legend'
    Assert-TraceContains $workspaceTrace.FullName '"enum member name"' 'mapped enum member token legend'
    Assert-TraceContains $workspaceTrace.FullName '"rlsNoStyleModifier"' 'mapped semantic token modifiers'
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
    Wait-Until {
        $servers = @(Get-OwnedServers | Where-Object ProcessId -ne $oldServerId)
        if ($servers.Count -eq 1) { return $servers[0] }
        return $null
    } 'exactly one replacement server' | Out-Null
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