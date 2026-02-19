#!/usr/bin/env bash
set -euo pipefail

########################################
# Defaults
########################################
BUILD_TYPE="Release"
STAGE_DIR=""
PROFILE="default"
CLEAN=0
GENERATOR=""
DEV_SDK=0

########################################
# Usage
########################################
usage() {
    echo "Usage: $0 -s <stage_dir> [-t Debug|Release] [-p <conan profile>] [-c] [-h]"
    echo ""
    echo "Options:"
    echo "  -s DIR        Production staging directory (required)"
    echo "  -t TYPE       Debug | Release (default: Release)"
    echo "  -p PROFILE    Conan profile (default: default)"
    echo "  -c            Clean build"
    echo "  -d            Include development symlink (libims.so)"
    echo "  -h            Help"
    exit 1
}

########################################
# Parse args
########################################
while getopts "s:t:p:cdh" opt; do
    case ${opt} in
        s) STAGE_DIR="$OPTARG" ;;
        t) BUILD_TYPE="$OPTARG" ;;
        p) PROFILE="$OPTARG" ;;
        c) CLEAN=1 ;;
        d) DEV_SDK=1 ;;   # development SDK
        h) usage ;;
        *) usage ;;
    esac
done

if [[ -z "$STAGE_DIR" ]]; then usage; fi
if [[ "$BUILD_TYPE" != "Debug" && "$BUILD_TYPE" != "Release" ]]; then
    echo "Build type must be Debug or Release"
    exit 1
fi

ROOT_BUILD_DIR="build"
BUILD_DIR="$ROOT_BUILD_DIR/$BUILD_TYPE"
TOOLCHAIN_FILE="$BUILD_DIR/generators/conan_toolchain.cmake"

########################################
# Select build generator
########################################
if command -v ninja >/dev/null 2>&1; then
    GENERATOR="Ninja"
elif command -v make >/dev/null 2>&1; then
    GENERATOR="Unix Makefiles"
else
    echo "Error: No suitable build tool found (Ninja or Make required)"
    exit 1
fi

echo "======================================="
echo "Build Type      : $BUILD_TYPE"
echo "Stage Dir       : $STAGE_DIR"
echo "Conan Profile   : $PROFILE"
echo "CMake Generator : $GENERATOR"
echo "======================================="

########################################
# Clean build
########################################
if [[ $CLEAN -eq 1 ]]; then
    echo "Cleaning build folders..."
    rm -rf "$ROOT_BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"

########################################
# Conan install
########################################
echo "Running Conan install..."
# Toolchain will be generated in build/generators
conan install . --profile "$PROFILE" -s build_type="$BUILD_TYPE" --build=missing

if [[ ! -f "$TOOLCHAIN_FILE" ]]; then
    echo "Error: Conan toolchain file not found: $TOOLCHAIN_FILE"
    exit 1
fi

########################################
# Configure CMake
########################################
echo "Configuring CMake..."
cmake -S . -B "$BUILD_DIR" -G "$GENERATOR" \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

########################################
# Build
########################################
echo "Building project..."
# Only pass --config for multi-config generators (Visual Studio)
if [[ "$GENERATOR" == "Ninja" || "$GENERATOR" == "Unix Makefiles" ]]; then
    cmake --build "$BUILD_DIR"
else
    cmake --build "$BUILD_DIR" --config "$BUILD_TYPE"
fi

########################################
# Stage artifacts
########################################
echo "Staging libraries..."

LIB_SRC_DIR="$BUILD_DIR"
LIB_DST_DIR="$STAGE_DIR/lib"

mkdir -p "$LIB_DST_DIR"

# Determine library base name
if [[ "$BUILD_TYPE" == "Debug" ]]; then
    LIB_BASE="libimsd"
else
    LIB_BASE="libims"
fi

# Find real versioned .so file (not symlink)
REAL_LIB=$(find "$LIB_SRC_DIR" -maxdepth 1 -type f -name "${LIB_BASE}.so.*" | sort | head -n 1)

if [[ -z "$REAL_LIB" ]]; then
    echo "Error: Could not locate versioned ${LIB_BASE}.so.*"
    exit 1
fi

REAL_LIB_NAME=$(basename "$REAL_LIB")

# Extract major version (e.g. 2 from libims.so.2.0.7)
MAJOR_VERSION=$(echo "$REAL_LIB_NAME" | sed -E 's/.*\.so\.([0-9]+).*/\1/')
SONAME="${LIB_BASE}.so.$MAJOR_VERSION"

# Copy real library
cp -fv "$REAL_LIB" "$LIB_DST_DIR/$REAL_LIB_NAME"

# Create SONAME symlink (runtime linker needs this)
ln -sf "$REAL_LIB_NAME" "$LIB_DST_DIR/$SONAME"

# Development symlink (optional)
if [[ $DEV_SDK -eq 1 ]]; then
    ln -sf "$REAL_LIB_NAME" "$LIB_DST_DIR/${LIB_BASE}.so"
fi
########################################
# Copy public headers from ims_api_h.cmake
########################################
CMAKE_HEADER_FILE="cmake/ims_api_h.cmake"
PUBLIC_HEADERS=()
INSIDE_SET=0

while IFS= read -r line; do
    line=$(echo "$line" | xargs)  # trim
    # Remove any trailing carriage return (\r)
    line=${line//$'\r'/}    
    if [[ "$line" =~ ^set\(\s*ims_public_api_header_files ]]; then
        INSIDE_SET=1
        continue
    fi
    if [[ $INSIDE_SET -eq 1 ]]; then
        if [[ "$line" =~ ^\) ]]; then
            INSIDE_SET=0
            continue
        fi
        # Strip ${api_include_dir}/ or ${api_lib_dir}/
        clean_line=$(echo "$line" | sed -E 's/\$\{[^}]+\}\///')
        if [[ -n "$clean_line" ]]; then
            PUBLIC_HEADERS+=("$clean_line")
        fi
    fi
done < "$CMAKE_HEADER_FILE"

# Copy public headers to staging area
for header in "${PUBLIC_HEADERS[@]}"; do
    src="include/$header"
    dest="$STAGE_DIR/include/$header"
    mkdir -p "$(dirname "$dest")"
    cp "$src" "$dest"
done

echo "======================================="
echo "Staging complete. Artifacts ready at '$STAGE_DIR'"
echo "======================================="
