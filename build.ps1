param(
    [switch]$Debug = $false,
    [switch]$ConfigOnly = $false,
    [switch]$BuildOnly = $false,
    [switch]$RunOnly = $false,
    [string]$Generator = "auto"  # "auto" | explicit CMake generator name | "default"
)

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$selectedGenerator = $null
$buildDir = $null
$buildType = if ($Debug) { "Debug" } else { "Release" }
$exePath = $null
$triplet = "x64-windows"

# Resolve vcpkg toolchain from VCPKG_ROOT when available; fall back to common path
$toolchain = $null
if ($env:VCPKG_ROOT) {
    $toolchainCandidate = Join-Path $env:VCPKG_ROOT "scripts/buildsystems/vcpkg.cmake"
    if (Test-Path $toolchainCandidate) { $toolchain = $toolchainCandidate }
}
if (-not $toolchain) {
    $fallbackToolchain = "C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
    if (Test-Path $fallbackToolchain) { $toolchain = $fallbackToolchain }
}

# Candidate SDL2 bin dirs (computed later after $buildDir is known)
$sdl2BinCandidates = @()

Write-Host "Stranded Build Script" -ForegroundColor Cyan
Write-Host "=====================" -ForegroundColor Cyan
Write-Host "Build Type: $buildType"
Write-Host ""

# Resolve build directory and generator
function Resolve-GeneratorAndDirs {
    param([string]$Gen)
    if ($Gen -eq "auto") {
        # Prefer Visual Studio 2026; fallback to default generator
        return @(
            @{ Name = "Visual Studio 18 2026"; BuildDir = (Join-Path $projectRoot "build-vs2026"); Arch = "x64" },
            @{ Name = $null; BuildDir = (Join-Path $projectRoot "build"); Arch = $null }
        )
    } elseif ($Gen -eq "default" -or [string]::IsNullOrWhiteSpace($Gen)) {
        return @(@{ Name = $null; BuildDir = (Join-Path $projectRoot "build"); Arch = $null })
    } else {
        # Use provided generator explicitly
        $dirName = ($Gen -replace '[^A-Za-z0-9]+','-').ToLower()
        $bd = Join-Path $projectRoot ("build-" + $dirName)
        # Assume VS-style needs -A x64; others don't
        $arch = if ($Gen -like "Visual Studio*") { "x64" } else { $null }
        return @(@{ Name = $Gen; BuildDir = $bd; Arch = $arch })
    }
}

$attempts = Resolve-GeneratorAndDirs -Gen $Generator

# Configure
if (-not $BuildOnly -and -not $RunOnly) {
    Write-Host "Configuring CMake..." -ForegroundColor Green
    $configured = $false
    foreach ($att in $attempts) {
        $selectedGenerator = $att.Name
        $buildDir = $att.BuildDir
        if (-not (Test-Path $buildDir)) { New-Item -ItemType Directory -Force -Path $buildDir | Out-Null }

        $cmakeArgs = @(
            '-S', $projectRoot,
            '-B', $buildDir,
            "-DCMAKE_BUILD_TYPE=$buildType",
            "-DVCPKG_TARGET_TRIPLET=$triplet"
        )
        if ($selectedGenerator) { $cmakeArgs += @('-G', $selectedGenerator) }
        if ($att.Arch) { $cmakeArgs += @('-A', $att.Arch) }

        if ($toolchain) {
            $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$toolchain"
        } else {
            Write-Warning "vcpkg toolchain file not found. Attempting local fallback."
            $localVcpkgPrefix = Join-Path $projectRoot "vcpkg_installed/$triplet"
            if (Test-Path $localVcpkgPrefix) {
                $cmakeArgs += "-DCMAKE_PREFIX_PATH=$($localVcpkgPrefix -replace '\\','/')"
                $sdl2Dir = Join-Path $localVcpkgPrefix "share/sdl2"
                if (Test-Path $sdl2Dir) {
                    $cmakeArgs += "-DSDL2_DIR=$($sdl2Dir -replace '\\','/')"
                }

                # Surface SDL2_net and SDL2_image when the toolchain is unavailable
                $sdl2NetDir = Join-Path $localVcpkgPrefix "share/sdl2-net"
                if (Test-Path $sdl2NetDir) {
                    $cmakeArgs += "-DSDL2_NET_DIR=$($sdl2NetDir -replace '\\','/')"
                }

                $sdl2ImageDir = Join-Path $localVcpkgPrefix "share/sdl2-image"
                if (Test-Path $sdl2ImageDir) {
                    $cmakeArgs += "-DSDL2_IMAGE_DIR=$($sdl2ImageDir -replace '\\','/')"
                }
            } else {
                Write-Warning "No local vcpkg_installed found at $localVcpkgPrefix. SDL2 discovery may fail."
            }
        }

        $genLabel = if ($null -eq $selectedGenerator) { 'default' } else { $selectedGenerator }
        Write-Host ("Generator: " + $genLabel) -ForegroundColor DarkCyan
        Write-Host "cmake " ($cmakeArgs -join ' ') -ForegroundColor DarkGray
        & cmake @cmakeArgs
        if ($LASTEXITCODE -eq 0) {
            $configured = $true
            break
        } else {
            $failGen = if ($null -eq $selectedGenerator) { 'default' } else { $selectedGenerator }
            Write-Warning "Configure failed with generator '$failGen'. Trying next option..."
        }
    }

    if (-not $configured) {
        Write-Host "CMake configuration failed for all generator attempts!" -ForegroundColor Red
        exit 1
    }

    # Resolve exe path now that buildDir is known
    $exePath = if ($Debug) { (Join-Path $buildDir "Debug"), "stranded.exe" -join "\" } else { (Join-Path $buildDir "Release"), "stranded.exe" -join "\" }

    if ($ConfigOnly) {
        Write-Host "Configuration complete." -ForegroundColor Green
        exit 0
    }
}

# Build
if (-not $RunOnly) {
    Write-Host ""
    Write-Host "Building..." -ForegroundColor Green
    if (-not $buildDir) {
        # Determine existing build directory when skipping configure
        if (Test-Path (Join-Path $projectRoot "build-vs2026")) {
            $buildDir = Join-Path $projectRoot "build-vs2026"
        } elseif (Test-Path (Join-Path $projectRoot "build")) {
            $buildDir = Join-Path $projectRoot "build"
        } else {
            # Fall back to generator resolution and pick the first attempt
            $attempts = Resolve-GeneratorAndDirs -Gen $Generator
            $buildDir = $attempts[0].BuildDir
        }
        $exePath = if ($Debug) { (Join-Path $buildDir "Debug"), "stranded.exe" -join "\" } else { (Join-Path $buildDir "Release"), "stranded.exe" -join "\" }
    }
    & cmake --build $buildDir --config $buildType
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Build failed!" -ForegroundColor Red
        exit 1
    }
    
    Write-Host "Build complete." -ForegroundColor Green
    
    if ($BuildOnly) {
        exit 0
    }
}

# Run
Write-Host ""
Write-Host "Running..." -ForegroundColor Green
if (-not $exePath) {
    if (-not $buildDir) {
        if (Test-Path (Join-Path $projectRoot "build-vs2026")) {
            $buildDir = Join-Path $projectRoot "build-vs2026"
        } elseif (Test-Path (Join-Path $projectRoot "build")) {
            $buildDir = Join-Path $projectRoot "build"
        } else {
            $attempts = Resolve-GeneratorAndDirs -Gen $Generator
            $buildDir = $attempts[0].BuildDir
        }
    }
    $exePath = if ($Debug) { (Join-Path $buildDir "Debug"), "stranded.exe" -join "\" } else { (Join-Path $buildDir "Release"), "stranded.exe" -join "\" }
}

# Prepend the first existing SDL2 bin path to PATH so the executable can find DLLs
# Recompute candidates now that $buildDir is known
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

& $exePath

if ($LASTEXITCODE -ne 0) {
    Write-Host "Execution failed with code $LASTEXITCODE" -ForegroundColor Red
    exit $LASTEXITCODE
}
