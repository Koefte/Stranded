param(
    [switch]$Debug = $false
)

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildType = if ($Debug) { "Debug" } else { "Release" }

Write-Host "Stranded Multiplayer Launcher" -ForegroundColor Cyan
Write-Host "=============================" -ForegroundColor Cyan
Write-Host ""

# Find the build directory
$buildDir = $null
if (Test-Path (Join-Path $projectRoot "build-vs2026")) {
    $buildDir = Join-Path $projectRoot "build-vs2026"
} elseif (Test-Path (Join-Path $projectRoot "build")) {
    $buildDir = Join-Path $projectRoot "build"
} else {
    Write-Host "Error: No build directory found. Please run build.ps1 first." -ForegroundColor Red
    exit 1
}

# Find the executable
$exePath = Join-Path $buildDir "$buildType\stranded.exe"
if (-not (Test-Path $exePath)) {
    Write-Host "Error: Executable not found at $exePath" -ForegroundColor Red
    Write-Host "Please run build.ps1 to compile the project first." -ForegroundColor Yellow
    exit 1
}

# Setup SDL2 DLL path
$sdl2BinCandidates = @(
    (Join-Path $buildDir "vcpkg_installed/x64-windows/bin"),
    (Join-Path $projectRoot "vcpkg_installed/x64-windows/bin"),
    ($(if ($env:VCPKG_ROOT) { Join-Path $env:VCPKG_ROOT "installed/x64-windows/bin" } else { $null })),
    "C:\\vcpkg\\installed\\x64-windows\\bin"
)
$resolvedSdlBin = $sdl2BinCandidates | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1
if ($resolvedSdlBin) {
    $env:PATH = "$resolvedSdlBin;" + $env:PATH
    Write-Host "Using SDL2 bin: $resolvedSdlBin" -ForegroundColor DarkGray
} else {
    Write-Warning "No SDL2 bin directory found. The app may fail to start if SDL2.dll is missing from PATH."
}

Write-Host ""
Write-Host "Launching Host..." -ForegroundColor Green
# Launch host in new window with proper arguments
Start-Process -FilePath $exePath -ArgumentList "--host", "9999" -WorkingDirectory $projectRoot

# Wait a moment for host to start
Start-Sleep -Seconds 2

Write-Host "Launching Client..." -ForegroundColor Green
# Launch client in new window with proper arguments
Start-Process -FilePath $exePath -ArgumentList "--connect", "127.0.0.1", "9999" -WorkingDirectory $projectRoot

Write-Host ""
Write-Host "Host and Client launched!" -ForegroundColor Cyan
Write-Host "Press any key to close both instances..." -ForegroundColor Yellow
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")

# Kill all stranded.exe processes
Write-Host "Closing instances..." -ForegroundColor Yellow
Get-Process -Name "stranded" -ErrorAction SilentlyContinue | Stop-Process -Force

Write-Host "Done." -ForegroundColor Green
