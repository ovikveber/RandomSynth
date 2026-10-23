param(
    [switch]$SkipBuild,
    [string]$InnoCompiler = ""
)

$ErrorActionPreference = 'Stop'

$isccCandidates = @(@(
    $InnoCompiler,
    'C:\Program Files\Inno Setup 7\ISCC.exe',
    'C:\Program Files (x86)\Inno Setup 6\ISCC.exe',
    'C:\Program Files\Inno Setup 6\ISCC.exe'
) | Where-Object { $_ -and (Test-Path -LiteralPath $_) })

if ($isccCandidates.Count -eq 0) {
    throw "ISCC.exe not found. Pass -InnoCompiler <path> or install Inno Setup 6/7."
}

$iscc = $isccCandidates[0]

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'build.ps1')
    if ($LASTEXITCODE -ne 0) {
        throw "Plugin build failed: $LASTEXITCODE"
    }
}

$iss = Join-Path $PSScriptRoot 'installer\RandomSynthMIDI.iss'
& $iscc $iss
if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup failed: $LASTEXITCODE"
}

$installer = Join-Path $PSScriptRoot 'dist\RandomSynth-MIDI-1.0.0-win-x64.exe'
if (-not (Test-Path -LiteralPath $installer)) {
    throw "Installer was not produced: $installer"
}

Write-Output "Installer OK -> $installer"
