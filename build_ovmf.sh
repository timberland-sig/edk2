#!/usr/bin/env bash
#
# Build script for OvmfPkgX64 with configurable NETWORK_* flags.
#
# Usage:
#   ./build_ovmf.sh [OPTIONS]
#
# Options:
#   -b DEBUG|RELEASE|NOOPT   Build target (default: DEBUG)
#   -t TOOLCHAIN             Toolchain tag (default: GCC on Linux)
#   -n THREADS               Number of build threads (default: 0 = auto)
#   --network on|off         Enable/disable entire network stack (default: on)
#   --snp on|off             SNP driver (default: on)
#   --ip4 on|off             IPv4 stack (default: on)
#   --ip6 on|off             IPv6 stack (default: on)
#   --tls on|off             TLS support (default: off per OvmfPkgX64.dsc)
#   --http on|off            HTTP(S) protocol (default: off)
#   --http-boot on|off       HTTP(S) boot (default: off per OvmfPkgX64.dsc)
#   --allow-http on|off      Allow plaintext HTTP (default: on per OvmfPkgX64.dsc)
#   --iscsi on|off           iSCSI support (default: on per OvmfPkgX64.dsc)
#   --iscsi-md5 on|off       iSCSI MD5 CHAP auth (default: on)
#   --nvmeof on|off          NVMe-oF over TCP (default: on)
#   --vlan on|off            VLAN support (default: on)
#   --pxe on|off             PXE boot (default: on)
#   --clean                  Run a clean build
#   -h, --help               Show this help
#
# Examples:
#   ./build_ovmf.sh
#   ./build_ovmf.sh -b RELEASE --nvmeof off --iscsi off
#   ./build_ovmf.sh --network off
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

#
# Defaults
#
BUILDTARGET=DEBUG
TARGET_TOOLS=
THREADNUMBER=0
CLEAN=no

# Network flag defaults match OvmfPkgX64.dsc + NetworkDefines.dsc.inc
NETWORK_ENABLE=TRUE
NETWORK_SNP_ENABLE=TRUE
NETWORK_IP4_ENABLE=TRUE
NETWORK_IP6_ENABLE=TRUE
NETWORK_TLS_ENABLE=TRUE
NETWORK_HTTP_ENABLE=FALSE
NETWORK_HTTP_BOOT_ENABLE=FALSE
NETWORK_ALLOW_HTTP_CONNECTIONS=TRUE
NETWORK_ISCSI_ENABLE=TRUE
NETWORK_ISCSI_MD5_ENABLE=FALSE
NETWORK_NVMEOF_ENABLE=TRUE
NETWORK_VLAN_ENABLE=TRUE
NETWORK_PXE_BOOT_ENABLE=TRUE

usage() {
    sed -n '3,/^$/s/^# \?//p' "$0"
    exit 0
}

bool_val() {
    case "$1" in
        on|ON|true|TRUE|1)   echo "TRUE" ;;
        off|OFF|false|FALSE|0) echo "FALSE" ;;
        *)
            echo "Error: invalid value '$1' (use on/off)" >&2
            exit 1
            ;;
    esac
}

#
# Parse arguments
#
while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--help)            usage ;;
        -b)                   BUILDTARGET="$2"; shift 2 ;;
        -t)                   TARGET_TOOLS="$2"; shift 2 ;;
        -n)                   THREADNUMBER="$2"; shift 2 ;;
        --clean)              CLEAN=yes; shift ;;
        --network)            NETWORK_ENABLE=$(bool_val "$2"); shift 2 ;;
        --snp)                NETWORK_SNP_ENABLE=$(bool_val "$2"); shift 2 ;;
        --ip4)                NETWORK_IP4_ENABLE=$(bool_val "$2"); shift 2 ;;
        --ip6)                NETWORK_IP6_ENABLE=$(bool_val "$2"); shift 2 ;;
        --tls)                NETWORK_TLS_ENABLE=$(bool_val "$2"); shift 2 ;;
        --http)               NETWORK_HTTP_ENABLE=$(bool_val "$2"); shift 2 ;;
        --http-boot)          NETWORK_HTTP_BOOT_ENABLE=$(bool_val "$2"); shift 2 ;;
        --allow-http)         NETWORK_ALLOW_HTTP_CONNECTIONS=$(bool_val "$2"); shift 2 ;;
        --iscsi)              NETWORK_ISCSI_ENABLE=$(bool_val "$2"); shift 2 ;;
        --iscsi-md5)          NETWORK_ISCSI_MD5_ENABLE=$(bool_val "$2"); shift 2 ;;
        --nvmeof)             NETWORK_NVMEOF_ENABLE=$(bool_val "$2"); shift 2 ;;
        --vlan)               NETWORK_VLAN_ENABLE=$(bool_val "$2"); shift 2 ;;
        --pxe)                NETWORK_PXE_BOOT_ENABLE=$(bool_val "$2"); shift 2 ;;
        *)
            echo "Unknown option: $1" >&2
            echo "Run '$0 --help' for usage." >&2
            exit 1
            ;;
    esac
done

#
# Detect toolchain
#
if [ -z "$TARGET_TOOLS" ]; then
    case "$(uname)" in
        Linux*)
            gcc_version=$(gcc -v 2>&1 | tail -1 | awk '{print $3}')
            case $gcc_version in
                [1-7].*)
                    echo "Error: GCC 8 or later is required (found $gcc_version)" >&2
                    exit 1
                    ;;
            esac
            TARGET_TOOLS=GCC
            ;;
        Darwin*)
            TARGET_TOOLS=XCODE5
            ;;
        *)
            echo "Error: unsupported platform $(uname). Set -t TOOLCHAIN manually." >&2
            exit 1
            ;;
    esac
fi

#
# Setup EDK2 workspace
#
if [ -z "$WORKSPACE" ]; then
    export EDK_TOOLS_PATH="$SCRIPT_DIR/BaseTools"
    source edksetup.sh BaseTools
fi

#
# Build BaseTools if needed
#
if ! command -v build &>/dev/null || ! command -v GenFv &>/dev/null; then
    echo "Building BaseTools..."
    make -C "$WORKSPACE/BaseTools"
elif [ ! -d "$EDK_TOOLS_PATH/Source/C/bin" ]; then
    echo "Building BaseTools..."
    make -C "$WORKSPACE/BaseTools"
fi

#
# Clean if requested
#
if [[ "$CLEAN" == "yes" ]]; then
    echo "Cleaning previous build..."
    build -p OvmfPkg/OvmfPkgX64.dsc -a X64 -b "$BUILDTARGET" -t "$TARGET_TOOLS" -n "$THREADNUMBER" clean || true
    echo "Clean complete."
    exit 0
fi

#
# Assemble -D flags
#
DFLAGS=(
    -D NETWORK_ENABLE="$NETWORK_ENABLE"
    -D NETWORK_SNP_ENABLE="$NETWORK_SNP_ENABLE"
    -D NETWORK_IP4_ENABLE="$NETWORK_IP4_ENABLE"
    -D NETWORK_IP6_ENABLE="$NETWORK_IP6_ENABLE"
    -D NETWORK_TLS_ENABLE="$NETWORK_TLS_ENABLE"
    -D NETWORK_HTTP_ENABLE="$NETWORK_HTTP_ENABLE"
    -D NETWORK_HTTP_BOOT_ENABLE="$NETWORK_HTTP_BOOT_ENABLE"
    -D NETWORK_ALLOW_HTTP_CONNECTIONS="$NETWORK_ALLOW_HTTP_CONNECTIONS"
    -D NETWORK_ISCSI_ENABLE="$NETWORK_ISCSI_ENABLE"
    -D NETWORK_ISCSI_MD5_ENABLE="$NETWORK_ISCSI_MD5_ENABLE"
    -D NETWORK_NVMEOF_ENABLE="$NETWORK_NVMEOF_ENABLE"
    -D NETWORK_VLAN_ENABLE="$NETWORK_VLAN_ENABLE"
    -D NETWORK_PXE_BOOT_ENABLE="$NETWORK_PXE_BOOT_ENABLE"
)

#
# Print configuration
#
echo "=========================================="
echo " OvmfPkgX64 Build Configuration"
echo "=========================================="
echo " Build target:  $BUILDTARGET"
echo " Toolchain:     $TARGET_TOOLS"
echo " Threads:       $THREADNUMBER (0 = auto)"
echo ""
echo " Network Flags:"
echo "   NETWORK_ENABLE                = $NETWORK_ENABLE"
echo "   NETWORK_SNP_ENABLE            = $NETWORK_SNP_ENABLE"
echo "   NETWORK_IP4_ENABLE            = $NETWORK_IP4_ENABLE"
echo "   NETWORK_IP6_ENABLE            = $NETWORK_IP6_ENABLE"
echo "   NETWORK_TLS_ENABLE            = $NETWORK_TLS_ENABLE"
echo "   NETWORK_HTTP_ENABLE           = $NETWORK_HTTP_ENABLE"
echo "   NETWORK_HTTP_BOOT_ENABLE      = $NETWORK_HTTP_BOOT_ENABLE"
echo "   NETWORK_ALLOW_HTTP_CONNECTIONS = $NETWORK_ALLOW_HTTP_CONNECTIONS"
echo "   NETWORK_ISCSI_ENABLE          = $NETWORK_ISCSI_ENABLE"
echo "   NETWORK_ISCSI_MD5_ENABLE      = $NETWORK_ISCSI_MD5_ENABLE"
echo "   NETWORK_NVMEOF_ENABLE         = $NETWORK_NVMEOF_ENABLE"
echo "   NETWORK_VLAN_ENABLE           = $NETWORK_VLAN_ENABLE"
echo "   NETWORK_PXE_BOOT_ENABLE       = $NETWORK_PXE_BOOT_ENABLE"
echo "=========================================="

#
# Build
#
echo "Building OvmfPkgX64..."
echo "build \
    -p OvmfPkg/OvmfPkgX64.dsc \
    -a X64 \
    -b \"$BUILDTARGET\" \
    -t \"$TARGET_TOOLS\" \
    -n \"$THREADNUMBER\" \
    \"${DFLAGS[@]}\""
build \
    -p OvmfPkg/OvmfPkgX64.dsc \
    -a X64 \
    -b "$BUILDTARGET" \
    -t "$TARGET_TOOLS" \
    -n "$THREADNUMBER" \
    "${DFLAGS[@]}"

BUILD_EXIT=$?

if [ $BUILD_EXIT -eq 0 ]; then
    BUILD_ROOT="$WORKSPACE/Build/OvmfX64/${BUILDTARGET}_${TARGET_TOOLS}"
    echo ""
    echo "Build succeeded."
    echo "Firmware image: $BUILD_ROOT/FV/OVMF.fd"
fi

exit $BUILD_EXIT
