#!/bin/bash
#
# Virtual Microphone HAL Integration Script
#
# Copies the virtual mic HAL into an AOSP tree. Auto-detects the Android
# version and selects the appropriate HAL adapter:
#   - Android 13 (Tiramisu): HIDL @7.1 adapter (hal/hidl-v7/)
#   - Android 14+:            AIDL Audio Core HAL V2 adapter (hal/aidl-v2/)
#
# Both adapters share the same core/ pipeline (shared memory, socket).
#
# Usage: ./integrate.sh /path/to/aosp
#
# Examples:
#   ./integrate.sh /mnt/micron/aosp-a13
#   ./integrate.sh /mnt/micron/aosp
#
# Override auto-detection:
#   HAL_VARIANT=hidl-v7 ./integrate.sh /path/to/aosp
#   HAL_VARIANT=aidl-v2 ./integrate.sh /path/to/aosp

set -e

AOSP_ROOT="${1:?Usage: $0 <aosp_root>}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
HAL_DEST="$AOSP_ROOT/hardware/interfaces/audio/virtualmic"

echo "=== Virtual Microphone HAL Integration ==="
echo "AOSP Root:        $AOSP_ROOT"
echo "HAL Destination:  $HAL_DEST"

# Verify AOSP root
if [[ ! -f "$AOSP_ROOT/build/envsetup.sh" ]]; then
    echo "ERROR: $AOSP_ROOT doesn't look like an AOSP tree"
    exit 1
fi

# Auto-detect HAL variant based on Android version in the repo manifest
HAL_VARIANT="${HAL_VARIANT:-}"
if [[ -z "$HAL_VARIANT" ]]; then
    if [[ -f "$AOSP_ROOT/.repo/manifests/default.xml" ]]; then
        if grep -q "android-13" "$AOSP_ROOT/.repo/manifests/default.xml"; then
            HAL_VARIANT="hidl-v7"
        else
            HAL_VARIANT="aidl-v2"
        fi
    else
        # Fallback: check for the AIDL audio core NDK lib in the tree
        if [[ -d "$AOSP_ROOT/hardware/interfaces/audio/aidl" ]]; then
            HAL_VARIANT="aidl-v2"
        else
            HAL_VARIANT="hidl-v7"
        fi
    fi
fi
echo "HAL Variant:      $HAL_VARIANT"
echo ""

if [[ "$HAL_VARIANT" != "hidl-v7" && "$HAL_VARIANT" != "aidl-v2" ]]; then
    echo "ERROR: Unknown HAL_VARIANT '$HAL_VARIANT' (expected hidl-v7 or aidl-v2)"
    exit 1
fi

# 1. Copy core + selected adapter
echo "[1/3] Copying HAL source..."
mkdir -p "$HAL_DEST/core" "$HAL_DEST/${HAL_VARIANT}"

rsync -a --delete "$REPO_ROOT/hal/core/" "$HAL_DEST/core/"
rsync -a --delete "$REPO_ROOT/hal/${HAL_VARIANT}/" "$HAL_DEST/${HAL_VARIANT}/"

# Remove any stale other-variant directory
if [[ "$HAL_VARIANT" == "hidl-v7" ]]; then
    rm -rf "$HAL_DEST/aidl-v2"
else
    rm -rf "$HAL_DEST/hidl-v7"
fi

echo "   ✓ Copied hal/core/ and hal/${HAL_VARIANT}/"

# Also install the test-renderer (virtualmic-renderer vendor daemon)
RENDERER_DEST="$AOSP_ROOT/vendor/virtualmic-renderer"
mkdir -p "$RENDERER_DEST"
rsync -a --delete "$REPO_ROOT/test-renderer/" "$RENDERER_DEST/"
echo "   ✓ Copied test-renderer/ → vendor/virtualmic-renderer/"

# 2. Apply version-specific fixups
if [[ "$HAL_VARIANT" == "hidl-v7" ]]; then
    echo "[2/3] Applying A13 HIDL fixups..."
    # Currently none needed — the hidl-v7 adapter is clean
    echo "   ✓ No fixups required"
else
    echo "[2/3] Applying A14+ AIDL fixups..."
    # The aidl-v2 adapter needs StreamWorker.h from audio-aidl-default
    # which is typically in the same dir; nothing to do if the default HAL is there
    echo "   ✓ No fixups required"
fi

# 3. Print next steps
echo "[3/3] Integration steps to complete manually:"
echo ""
echo "   Device makefile (e.g., device/google/cuttlefish/shared/device.mk):"
echo "     PRODUCT_PACKAGES += android.hardware.audio.service.virtualmic"
echo ""
echo "   Build:"
echo "     cd $AOSP_ROOT"
echo "     source build/envsetup.sh"
if [[ "$HAL_VARIANT" == "hidl-v7" ]]; then
    echo "     lunch aosp_cf_x86_64_phone-userdebug"
    echo "     m android.hardware.audio.service.virtualmic"
    echo ""
    echo "   The HIDL @7.1 HAL will register as:"
    echo "     @7.1::IDevicesFactory/virtualmic"
else
    echo "     lunch aosp_cf_x86_64_only_phone-trunk_staging-userdebug"
    echo "     m android.hardware.audio.service.virtualmic"
    echo ""
    echo "   The AIDL Audio Core HAL V2 service will register as:"
    echo "     android.hardware.audio.core.IModule/virtualmic"
fi
echo ""
echo "   SELinux policies (sepolicy/) and audio policy config"
echo "   (virtualmic_audio_policy_configuration.xml from aosp-integration/)"
echo "   should be added to the device sepolicy and audiopolicy dirs."
echo ""
echo "=== Integration Complete (${HAL_VARIANT}) ==="
