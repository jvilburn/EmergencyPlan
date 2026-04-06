# Build script that sets up MSVC and Qt environment for PowerShell
param(
    [string]$Target = "",
    [switch]$Clean,
    [switch]$Run  # Run the target after building
)

# Add Qt to PATH for runtime DLL loading
$env:PATH = "C:\Qt\6.10.1\msvc2022_64\bin;$env:PATH"

# Import Visual Studio Developer Shell
$vsPath = "C:\Program Files\Microsoft Visual Studio\2022\Community"
$devShellModule = "$vsPath\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"

if (-not (Test-Path $devShellModule)) {
    Write-Error "Could not find Visual Studio DevShell module at $devShellModule"
    exit 1
}

Import-Module $devShellModule
Enter-VsDevShell -VsInstallPath $vsPath -DevCmdArguments '-arch=x64' -SkipAutomaticLocation | Out-Null

Write-Host "Building..." -ForegroundColor Cyan

$buildArgs = @("--build", "build")
if ($Target) {
    $buildArgs += "--target"
    $buildArgs += $Target
}
if ($Clean) {
    $buildArgs += "--clean-first"
}

& cmake @buildArgs
$buildExitCode = $LASTEXITCODE

Write-Host "Build complete with exit code $buildExitCode" -ForegroundColor $(if ($buildExitCode -eq 0) { "Green" } else { "Red" })

# Optionally run the target
if ($Run -and $buildExitCode -eq 0 -and $Target) {
    $exePath = "build\bin\$Target.exe"
    if (Test-Path $exePath) {
        Write-Host "`nRunning $Target..." -ForegroundColor Cyan
        & ".\$exePath"
    }
}

exit $buildExitCode
