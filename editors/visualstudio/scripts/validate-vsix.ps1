#requires -Version 5.1

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$VsixPath,

    [string]$ExpectedTag
)

$ErrorActionPreference = 'Stop'

$visualStudioDirectory = Split-Path -Parent $PSScriptRoot
$repositoryDirectory = Resolve-Path (Join-Path $visualStudioDirectory '..\..')
$packageJsonPath = Join-Path $repositoryDirectory 'editors\vscode\package.json'
$nativeValidatorPath = Join-Path $repositoryDirectory 'editors\shared\validate-native-binary.mjs'
$resolvedVsixPath = (Resolve-Path -LiteralPath $VsixPath).Path
$expectedVersion = (Get-Content -LiteralPath $packageJsonPath -Raw | ConvertFrom-Json).version

if ([string]::IsNullOrWhiteSpace($ExpectedTag) -and $env:GITHUB_REF_TYPE -eq 'tag') {
    $ExpectedTag = $env:GITHUB_REF_NAME
}

Add-Type -AssemblyName System.IO.Compression.FileSystem

function Read-ZipEntryText {
    param([System.IO.Compression.ZipArchiveEntry]$Entry)

    $reader = [System.IO.StreamReader]::new($Entry.Open())
    try {
        return $reader.ReadToEnd()
    }
    finally {
        $reader.Dispose()
    }
}

function Require-ZipEntry {
    param(
        [System.IO.Compression.ZipArchive]$Archive,
        [string]$Name
    )

    $entry = $Archive.GetEntry($Name)
    if ($null -eq $entry) {
        throw "VSIX is missing required entry '$Name'."
    }
    return $entry
}

$archive = [System.IO.Compression.ZipFile]::OpenRead($resolvedVsixPath)
$temporaryServerPath = $null
try {
    $manifestEntry = Require-ZipEntry $archive 'extension.vsixmanifest'
    $packageManifestEntry = Require-ZipEntry $archive 'manifest.json'
    [xml]$manifest = Read-ZipEntryText $manifestEntry
    $packageManifest = Read-ZipEntryText $packageManifestEntry | ConvertFrom-Json

    $namespace = [System.Xml.XmlNamespaceManager]::new($manifest.NameTable)
    $namespace.AddNamespace('v', 'http://schemas.microsoft.com/developer/vsx-schema/2011')

    $identity = $manifest.SelectSingleNode('/v:PackageManifest/v:Metadata/v:Identity', $namespace)
    if ($null -eq $identity -or $identity.Id -ne 'RandoLogicScript.VisualStudio') {
        throw "VSIX has an unexpected identity '$($identity.Id)'."
    }
    if ($identity.Version -ne $expectedVersion) {
        throw "Visual Studio VSIX version '$($identity.Version)' does not match VS Code version '$expectedVersion'."
    }
    if ($packageManifest.id -ne $identity.Id -or $packageManifest.version -ne $identity.Version) {
        throw 'Generated manifest.json identity does not match extension.vsixmanifest.'
    }

    if (-not [string]::IsNullOrWhiteSpace($ExpectedTag)) {
        $normalizedTag = $ExpectedTag -replace '^refs/tags/', ''
        if ($normalizedTag -ne "v$expectedVersion") {
            throw "Release tag '$ExpectedTag' does not match extension version 'v$expectedVersion'."
        }
    }

    $installationTarget = $manifest.SelectSingleNode(
        '/v:PackageManifest/v:Installation/v:InstallationTarget',
        $namespace)
    if ($null -eq $installationTarget -or
        $installationTarget.Id -ne 'Microsoft.VisualStudio.Community' -or
        $installationTarget.Version -ne '[17.0,18.0)') {
        throw 'VSIX must target Microsoft.VisualStudio.Community [17.0,18.0).'
    }
    $architecture = $installationTarget.SelectSingleNode('v:ProductArchitecture', $namespace)
    if ($null -eq $architecture -or $architecture.InnerText -ne 'amd64') {
        throw "VSIX product architecture must be amd64; found '$($architecture.InnerText)'."
    }

    $assetNodes = $manifest.SelectNodes('/v:PackageManifest/v:Assets/v:Asset', $namespace)
    $assetsByType = @{}
    foreach ($asset in $assetNodes) {
        $assetsByType[$asset.Type] = $asset.Path
    }
    if ($assetsByType['Microsoft.VisualStudio.MefComponent'] -ne 'RandoLogicScript.VisualStudio.dll') {
        throw 'VSIX is missing the RandoLogicScript.VisualStudio.dll MEF asset.'
    }
    if ($assetsByType['Microsoft.VisualStudio.VsPackage'] -ne 'RandoLogicScript.pkgdef') {
        throw 'VSIX is missing the RandoLogicScript.pkgdef package asset.'
    }

    $requiredEntries = @(
        'RandoLogicScript.VisualStudio.dll',
        'RandoLogicScript.pkgdef',
        'Grammars/rls.tmLanguage.json',
        'language-configuration.json',
        'LICENSE.txt',
        'Snippets/1033/RandoLogicScript.xml',
        'Snippets/1033/region.snippet',
        'Snippets/1033/extend-region.snippet',
        'Snippets/1033/enum.snippet',
        'Snippets/1033/events.snippet',
        'Snippets/1033/locations.snippet',
        'Snippets/1033/exits.snippet',
        'Server/rls_language_server.exe'
    )
    foreach ($entryName in $requiredEntries) {
        Require-ZipEntry $archive $entryName | Out-Null
    }

    $expectedSnippets = @{
        'Snippets/1033/region.snippet' = 'region'
        'Snippets/1033/extend-region.snippet' = 'extend'
        'Snippets/1033/enum.snippet' = 'enum'
        'Snippets/1033/events.snippet' = 'events'
        'Snippets/1033/locations.snippet' = 'locations'
        'Snippets/1033/exits.snippet' = 'exits'
    }
    foreach ($snippetEntryName in $expectedSnippets.Keys) {
        [xml]$snippet = Read-ZipEntryText (Require-ZipEntry $archive $snippetEntryName)
        $snippetNamespace = [System.Xml.XmlNamespaceManager]::new($snippet.NameTable)
        $snippetNamespace.AddNamespace('s', 'http://schemas.microsoft.com/VisualStudio/2005/CodeSnippet')
        $codeSnippet = $snippet.SelectSingleNode('/s:CodeSnippets/s:CodeSnippet', $snippetNamespace)
        $shortcut = $codeSnippet.SelectSingleNode('s:Header/s:Shortcut', $snippetNamespace)
        $code = $codeSnippet.SelectSingleNode('s:Snippet/s:Code', $snippetNamespace)
        if ($null -eq $codeSnippet -or $shortcut.InnerText -ne $expectedSnippets[$snippetEntryName] -or
            $code.Language -ne 'RLS' -or $code.InnerText -notmatch '\$end\$') {
            throw "VSIX snippet '$snippetEntryName' has invalid RLS expansion metadata."
        }
    }

    [xml]$snippetIndex = Read-ZipEntryText (
        Require-ZipEntry $archive 'Snippets/1033/RandoLogicScript.xml')
    $snippetLanguage = $snippetIndex.SelectSingleNode('/SnippetCollection/Language')
    if ($null -eq $snippetLanguage -or
        $snippetLanguage.Lang -ne 'Rando Logic Script' -or
        $snippetLanguage.Guid -ne '{4C159F73-D995-4A40-ACD4-A04BB3DE2118}') {
        throw 'VSIX native snippet index has invalid RLS language registration.'
    }

    $serverEntries = @($archive.Entries | Where-Object {
        -not [string]::IsNullOrEmpty($_.Name) -and $_.FullName.StartsWith('Server/')
    })
    if ($serverEntries.Count -ne 1 -or
        $serverEntries[0].FullName -ne 'Server/rls_language_server.exe') {
        $found = ($serverEntries.FullName -join ', ')
        throw "Expected only Server/rls_language_server.exe; found '$found'."
    }

    $declaredFiles = @($packageManifest.files.fileName)
    foreach ($entryName in $requiredEntries) {
        if ("/$entryName" -notin $declaredFiles) {
            throw "Generated manifest.json does not declare '/$entryName'."
        }
    }

    $temporaryServerPath = Join-Path ([System.IO.Path]::GetTempPath()) (
        "rls-vsix-server-$([guid]::NewGuid().ToString('N')).exe")
    $source = $serverEntries[0].Open()
    $destination = [System.IO.File]::Create($temporaryServerPath)
    try {
        $source.CopyTo($destination)
    }
    finally {
        $destination.Dispose()
        $source.Dispose()
    }

    & node $nativeValidatorPath $temporaryServerPath win32-x64
    if ($LASTEXITCODE -ne 0) {
        throw "Native server validation failed with exit code $LASTEXITCODE."
    }

    Write-Output "Validated $([System.IO.Path]::GetFileName($resolvedVsixPath)) for Visual Studio amd64 version $expectedVersion."
}
finally {
    $archive.Dispose()
    if ($null -ne $temporaryServerPath -and (Test-Path -LiteralPath $temporaryServerPath)) {
        Remove-Item -LiteralPath $temporaryServerPath -Force
    }
}