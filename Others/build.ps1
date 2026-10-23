$ErrorActionPreference = 'Stop'

$src    = $PSScriptRoot
$work   = Join-Path $env:TEMP 'RandomSynthJUCE'
$dist   = Join-Path $src 'dist'
$cmake  = 'D:\VS studio\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ninja  = 'D:\clion\CLion 2025.2.2\bin\ninja\win\x64\ninja.exe'
$vcvars = 'D:\VS studio\VC\Auxiliary\Build\vcvars64.bat'

Get-Process RandomSynth -ErrorAction SilentlyContinue | Stop-Process -Force

New-Item -ItemType Directory -Path $work, $dist -Force | Out-Null

Copy-Item -LiteralPath (Join-Path $src 'CMakeLists.txt') -Destination $work -Force
if (Test-Path -LiteralPath (Join-Path $work 'src')) {
    Remove-Item -LiteralPath (Join-Path $work 'src') -Recurse -Force
}
Copy-Item -LiteralPath (Join-Path $src 'src') -Destination $work -Recurse -Force

$lines = & cmd.exe /c "call `"$vcvars`" >nul 2>&1 && set"
foreach ($line in $lines) {
    if ($line -match '^([^=]+)=(.*)$') {
        Set-Item -Path ("Env:" + $matches[1]) -Value $matches[2] -ErrorAction SilentlyContinue
    }
}

Push-Location $work
try {
    & $cmake -S $work -B (Join-Path $work 'build') -G Ninja `
        -DCMAKE_MAKE_PROGRAM="$ninja" -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed: $LASTEXITCODE" }

    & $cmake --build (Join-Path $work 'build') --config Release
    if ($LASTEXITCODE -ne 0) { throw "Build failed: $LASTEXITCODE" }
}
finally {
    Pop-Location
}

$exe = Join-Path $work 'build\RandomSynth_artefacts\Release\Standalone\RandomSynth.exe'
Copy-Item -LiteralPath $exe -Destination (Join-Path $dist 'RandomSynth.exe') -Force

$midiExe = Join-Path $work 'build\RandomSynthMIDI_artefacts\Release\Standalone\RandomSynth MIDI.exe'
if (Test-Path -LiteralPath $midiExe) {
    Copy-Item -LiteralPath $midiExe -Destination (Join-Path $dist 'RandomSynth MIDI.exe') -Force
}

$vst3 = Join-Path $work 'build\RandomSynthMIDI_artefacts\Release\VST3\RandomSynth MIDI.vst3'
$vst3Dest = Join-Path $dist 'RandomSynth MIDI.vst3'
if (Test-Path -LiteralPath $vst3) {
    if (Test-Path -LiteralPath $vst3Dest) {
        Remove-Item -LiteralPath $vst3Dest -Recurse -Force
    }
    Copy-Item -LiteralPath $vst3 -Destination $vst3Dest -Recurse -Force
}

Write-Output "Build OK -> $dist\RandomSynth.exe"
Write-Output "Build OK -> $dist\RandomSynth MIDI.vst3"
