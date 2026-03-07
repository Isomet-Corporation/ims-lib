<#
.SYNOPSIS
    Build IMS API library and stage production artifacts for SDK packaging.

.PARAMETER BuildType
    Build configuration: Release (default) or Debug

.PARAMETER Arch
    Target architecture: x86 (32-bit) or x86_64 (default)

.PARAMETER BuildDocs
    Switch to build documentation only

.PARAMETER CompilerVersion
    Compiler version to pass to Conan (e.g., 192 for Visual Studio 2019)

.DESCRIPTION
    - Builds Debug or Release using Visual Studio / CMake
    - Uses Conan 2.x for dependencies
    - Stages binaries, libraries, and only public headers into a staging folder
#>

[CmdletBinding()]
param(
    [string]$StageDir,

    [ValidateSet("Debug","Release")]
    [string]$BuildType = "Release",   # Debug or Release

    [ValidateSet("x86","x86_64")]
    [string]$Arch = "x86_64",

    [switch]$BuildDocs,               # Optional switch to build docs

    [string]$CompilerVersion = "",

    [string]$Profile = "default",
    [switch]$Clean
)

if (-not $StageDir) {
    Write-Host "Usage: .\build.ps1 -StageDir <path> [-BuildType Debug|Release] [-BuildDocs]
      [-CompilerVersion <version>] [-Profile <conan profile>] [-Clean]"
    exit 1
}

# Detect OS architecture
$OSName = "windows"
#$Arch = if ([Environment]::Is64BitOperatingSystem) { "x64" } else { "x86" }
$OSArch = "${OSName}_${Arch}"

#$BuildDir = Join-Path "build" $BuildType
$BuildDir = "build"
$IncludeDir = Join-Path $StageDir "include"
$LibDir     = Join-Path (Join-Path $StageDir $OSArch) "lib"
$DocsDir    = Join-Path $StageDir "docs"

Write-Host "========================================"
Write-Host "Build Type    : $BuildType"
Write-Host "Stage Dir     : $StageDir"
Write-Host "Architecture  : $Arch"
Write-Host "Profile       : $Profile"
Write-Host "Build Docs    : $BuildDocs"
Write-Host "Clean         : $Clean"
Write-Host "========================================"

# ----------------------------
# Clean
# ----------------------------
if ($Clean) {
    if (Test-Path $BuildDir) {
        Write-Host "Cleaning build directory..."
        Remove-Item -Recurse -Force $BuildDir -ErrorAction SilentlyContinue
    }
}

# -------------------------------------------------------
# Sync version
# -------------------------------------------------------
$PythonCmd = "python"
$HEADER_FILE = ".\include\LibVersion.h"

& $PythonCmd sync_version.py $HEADER_FILE

# ----------------------------
# Conan install
# ----------------------------
$ConanCmd = @(
    "conan install .",
    "--profile $Profile",
    "-s build_type=$BuildType",
    "-s arch=$Arch",
    "-s compiler.cppstd=17",
    "--build=missing",
    "-of $BuildDir"
)

if ($CompilerVersion) { $ConanCmd += "-s compiler.version=$CompilerVersion" }

Write-Host "Running Conan install..."
Write-Host $ConanCmd -join " "
Invoke-Expression ($ConanCmd -join " ")
#conan install . --profile $Profile --settings build_type=$BuildType --build=missing -of $BuildDir

# Toolchain file location (always in build/generators)
$ToolchainFile = "build\generators\conan_toolchain.cmake"

if (-not $BuildDocs) {
    # ----------------------------
    # Configure & Build
    # ----------------------------
    Write-Host "Configuring CMake..."
    $CMakeArgs = @(
        "-S .",
        "-B $BuildDir",
        "-DCMAKE_TOOLCHAIN_FILE=$BuildDir\generators\conan_toolchain.cmake",
        "-DCMAKE_BUILD_TYPE=$BuildType"
    )

    # Specify architecture for Visual Studio
    if ($Arch -eq "x86") {
        $CMakeArgs += "-A Win32"
    } elseif ($Arch -eq "x86_64") {
        $CMakeArgs += "-A x64"
    }
    & cmake @CMakeArgs

    Write-Host "Building project..."
    cmake --build $BuildDir --config $BuildType

    # ----------------------------
    # Stage artifacts
    # ----------------------------
    Write-Host "Staging include headers -> $IncludeDir"
    Write-Host "Staging libraries -> $LibDir"
#    if (Test-Path $StageDir) { Remove-Item -Recurse -Force $StageDir }
    New-Item -ItemType Directory -Force -Path $IncludeDir, $LibDir, $DocsDir | Out-Null

    # Copy binaries
    $LibSource = Join-Path $BuildDir "$BuildType"

    if (Test-Path $LibSource) { Copy-Item -Path "$LibSource\*.dll" -Destination "$LibDir" -Recurse -Force }
    if (Test-Path $LibSource) { Copy-Item -Path "$LibSource\*.lib" -Destination "$LibDir" -Recurse -Force }

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
        $dest = Join-Path $IncludeDir $header
        New-Item -ItemType Directory -Path (Split-Path $dest) -Force | Out-Null
        Copy-Item $src $dest -Force
    }
} else {
    Write-Host "Configuring CMake for Documentation..."
    cmake -S . -B $BuildDir -DBUILD_DOCS=ON `
        -DCMAKE_TOOLCHAIN_FILE="$ToolchainFile" `
        -DCMAKE_BUILD_TYPE=$BuildType

    # === Build Documentation Only ===
    Write-Host "Building documentation (HTML)..."
    cmake --build $BuildDir --target docs --config $BuildType

    Write-Host "Building PDF documentation..."
    try {
        cmake --build $BuildDir --target docs_pdf --config $BuildType
    } catch {
        Write-Warning "PDF build failed, skipping..."
    }

    # Read SDK version from CMake
    $VersionFile = Join-Path $BuildDir "CMakeCache.txt"
    if (Test-Path $VersionFile) {
        $ProjectNumber = Select-String -Path $VersionFile -Pattern "PROJECT_VERSION:STATIC=" | ForEach-Object {
            ($_ -split "=")[1].Trim()
        }
    } else {
        $ProjectNumber = "0.0.0"
    }

    # Copy HTML and PDF to staging
    Copy-Item -Recurse -Force "$BuildDir/docs/html/*" "$DocsDir/html"
    $PdfFile = Join-Path $BuildDir "docs/latex/refman.pdf"
    if (Test-Path $PdfFile) {
        $VersionedPdf = Join-Path $DocsDir "IMS_SDK_v${ProjectNumber}_${BuildType}.pdf"
        Copy-Item -Force $PdfFile $VersionedPdf
    }
}

Write-Host "Staging complete."
Write-Host "Production artifacts are ready in '$StageDir'."
