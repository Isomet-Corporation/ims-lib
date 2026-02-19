<#
.SYNOPSIS
    Build IMS API library and stage production artifacts for SDK packaging.
.DESCRIPTION
    - Builds Debug or Release using Visual Studio / CMake
    - Uses Conan 2.x for dependencies
    - Stages binaries, libraries, and only public headers into a staging folder
#>

Param(
    [string]$StageDir,
    [ValidateSet("Debug","Release")]
    [string]$BuildType = "Release",
    [string]$Profile = "default",
    [switch]$Clean
)

if (-not $StageDir) {
    Write-Host "Usage: .\build.ps1 -StageDir <path> [-BuildType Debug|Release] [-Profile <conan profile>] [-Clean]"
    exit 1
}

#$BuildDir = Join-Path "build" $BuildType
$BuildDir = "build"

Write-Host "========================================"
Write-Host "Build Type : $BuildType"
Write-Host "Stage Dir  : $StageDir"
Write-Host "Profile    : $Profile"
Write-Host "========================================"

# ----------------------------
# Clean
# ----------------------------
if ($Clean) {
    if (Test-Path $BuildDir) {
        Write-Host "Cleaning build directory..."
        Remove-Item -Recurse -Force $BuildDir
    }
}

# ----------------------------
# Conan install
# ----------------------------
Write-Host "Running Conan install..."
conan install . --profile $Profile --settings build_type=$BuildType --build=missing -of $BuildDir

# Toolchain file location (always in build/generators)
$ToolchainFile = "build\generators\conan_toolchain.cmake"

# ----------------------------
# Configure & Build
# ----------------------------
Write-Host "Configuring CMake..."
cmake -S . -B $BuildDir -G "Visual Studio 17 2022" `
    -DCMAKE_TOOLCHAIN_FILE="$ToolchainFile" `
    -DCMAKE_BUILD_TYPE=$BuildType

Write-Host "Building project..."
cmake --build $BuildDir --config $BuildType

# ----------------------------
# Stage artifacts
# ----------------------------
Write-Host "Staging artifacts..."
if (Test-Path $StageDir) { Remove-Item -Recurse -Force $StageDir }
New-Item -ItemType Directory -Path "$StageDir\lib" -Force | Out-Null
New-Item -ItemType Directory -Path "$StageDir\include" -Force | Out-Null

# Copy binaries
$LibSource = Join-Path $BuildDir "$BuildType"

if (Test-Path $LibSource) { Copy-Item -Path "$LibSource\*.dll" -Destination "$StageDir\lib" -Recurse -Force }
if (Test-Path $LibSource) { Copy-Item -Path "$LibSource\*.lib" -Destination "$StageDir\lib" -Recurse -Force }

# ----------------------------
# Copy public headers dynamically from ims_api_h.cmake
# ----------------------------
$CMakeHeaderFile = "cmake\ims_api_h.cmake"
$PublicHeaders = @()
$InsideSet = $false

Get-Content $CMakeHeaderFile | ForEach-Object {
    $line = $_.Trim()
    if ($line -match "^set\s*\(\s*ims_public_api_header_files") {
        $InsideSet = $true
        return
    }
    if ($InsideSet) {
        if ($line -match "^\)") {
            $InsideSet = $false
            return
        }
        # Remove ${api_include_dir}/ or ${api_lib_dir}/
        $cleanLine = $line -replace "\$\{[^\}]+\}/", ""
        if ($cleanLine -ne "") { $PublicHeaders += $cleanLine }
    }
}

# Copy public headers to staging area
foreach ($header in $PublicHeaders) {
    $src = Join-Path "include" $header
    $dest = Join-Path $StageDir "include\$header"
    New-Item -ItemType Directory -Path (Split-Path $dest) -Force | Out-Null
    Copy-Item $src $dest -Force
}

Write-Host "Staging complete."
Write-Host "Production artifacts are ready in '$StageDir'."
