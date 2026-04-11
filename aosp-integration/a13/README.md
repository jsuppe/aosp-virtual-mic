# Android 13 Cuttlefish Integration

Device-side artifacts needed to run the Virtual Microphone HAL on an Android 13
Cuttlefish tree. These edits are **not** applied by `scripts/integrate.sh` —
they live outside the HAL source tree (in `device/google/cuttlefish`,
`device/generic/goldfish`, and `frameworks/av`), so they have to be applied by
hand (or by a separate device-mk edit script) once per AOSP checkout.

`integrate.sh` handles the HAL and renderer sources; this directory handles
everything else needed to make A13 Cuttlefish actually pick up and boot with
the HAL enabled.

## Layout

```
a13/
├── cuttlefish-sepolicy/
│   └── virtualmic.te                           → device/google/cuttlefish/shared/sepolicy/vendor/
├── cuttlefish-config/
│   └── device_framework_matrix_virtualmic.xml  → device/google/cuttlefish/shared/config/
├── audiopolicy/
│   └── virtualmic_audio_policy_configuration.xml → frameworks/av/services/audiopolicy/config/
└── patches/
    ├── 0001-cuttlefish-device.mk.patch               (device/google/cuttlefish)
    ├── 0002-cuttlefish-file_contexts.patch           (device/google/cuttlefish)
    ├── 0003-goldfish-ranchu-manifest.patch           (device/generic/goldfish)
    └── 0004-goldfish-audio_policy_configuration.patch (device/generic/goldfish)
```

## What each piece does

- **`virtualmic.te`** — Defines the `virtualmic_data_file`, `virtualmic_renderer`
  domain and `virtualmic_renderer_exec` type. Allows the HAL (`hal_audio_default`)
  to create/bind the Unix socket in `/data/vendor/virtualmic/` and to `use` the
  ashmem fd passed over SCM_RIGHTS from the renderer domain. Allows the renderer
  to `connectto` the HAL socket (vendor-to-vendor — core-to-vendor is blocked by
  Treble's neverallow). `init_daemon_domain(virtualmic_renderer)` makes the init
  service transition into the right domain at exec time.

- **`device_framework_matrix_virtualmic.xml`** — Adds a device-level framework
  compatibility matrix entry declaring that the framework accepts
  `@7.0-1::IDevicesFactory/virtualmic`. Without this, `checkvintf` rejects the
  vendor manifest at boot because the framework side has no matching matrix
  entry for the `virtualmic` instance.

- **`virtualmic_audio_policy_configuration.xml`** — Audio policy module for the
  virtual mic. Declares a single input mix port wired to an
  `AUDIO_DEVICE_IN_BUS` device port with address `virtual_mic_0`. This is what
  `AudioManager.getDevices(GET_DEVICES_INPUTS)` will return to the app, and
  what `setPreferredDevice()` is called with.

- **`0001-cuttlefish-device.mk.patch`** — Adds `android.hardware.audio.service.virtualmic`
  and `virtualmic-renderer` to `PRODUCT_PACKAGES`, copies the audio policy XML
  into `$(TARGET_COPY_OUT_VENDOR)/etc/`, and appends the device framework
  matrix to `DEVICE_FRAMEWORK_COMPATIBILITY_MATRIX_FILE`.

- **`0002-cuttlefish-file_contexts.patch`** — Labels the HAL binary as
  `hal_audio_default_exec`, the renderer binary as `virtualmic_renderer_exec`,
  and `/data/vendor/virtualmic/` as `virtualmic_data_file`. Without these, the
  HAL boots in `vendor_file` domain and SELinux blocks everything.

- **`0003-goldfish-ranchu-manifest.patch`** — Adds `<instance>virtualmic</instance>`
  alongside `default` in the ranchu audio HAL manifest fragment. This merges the
  two instances into a **single** HIDL `@7.1::IDevicesFactory` entry in the
  vendor manifest. Declaring them as separate fragments trips the "two fragments
  with the same major version" VINTF check at boot.

- **`0004-goldfish-audio_policy_configuration.patch`** — Adds
  `<xi:include href="virtualmic_audio_policy_configuration.xml"/>` to the
  goldfish audio policy aggregator so audioserver actually picks up the module.

## Apply

From an Android 13 AOSP checkout at `$AOSP`:

```sh
REPO=/path/to/aosp-virtual-mic/aosp-integration/a13

# Source files (copy into place)
cp "$REPO/cuttlefish-sepolicy/virtualmic.te" \
   "$AOSP/device/google/cuttlefish/shared/sepolicy/vendor/"
cp "$REPO/cuttlefish-config/device_framework_matrix_virtualmic.xml" \
   "$AOSP/device/google/cuttlefish/shared/config/"
cp "$REPO/audiopolicy/virtualmic_audio_policy_configuration.xml" \
   "$AOSP/frameworks/av/services/audiopolicy/config/"

# Patches (apply in the appropriate project roots)
cd "$AOSP/device/google/cuttlefish" && \
    patch -p1 < "$REPO/patches/0001-cuttlefish-device.mk.patch" && \
    patch -p1 < "$REPO/patches/0002-cuttlefish-file_contexts.patch"

cd "$AOSP/device/generic/goldfish" && \
    patch -p1 < "$REPO/patches/0003-goldfish-ranchu-manifest.patch" && \
    patch -p1 < "$REPO/patches/0004-goldfish-audio_policy_configuration.patch"
```

Then run the HAL integration:

```sh
cd /path/to/aosp-virtual-mic
bash scripts/integrate.sh "$AOSP"   # copies hal/ and test-renderer/
```

Build and launch:

```sh
cd "$AOSP"
source build/envsetup.sh
lunch aosp_cf_x86_64_phone-userdebug
m
launch_cvd --memory_mb=6144 --cpus=4
```

## Verification

After the device boots, `dumpsys media.audio_policy` should show the
`virtualmic` module with a `virtual_mic_0` input device. Apps enumerating with
`AudioManager.getDevices(GET_DEVICES_INPUTS)` will see it as
`id=<N>, type=BUS, address=virtual_mic_0`.

The renderer launches automatically as `vendor.virtualmic-renderer` at
`sys.boot_completed=1` and streams a 440 Hz sine wave into the HAL's ring
buffer indefinitely.

Measured end-to-end latency (renderer → ring buffer → HAL → AudioFlinger →
AudioRecord) on A13 Cuttlefish: **~4.8 ms**, derived from waveform phase
correlation in the `unified-test` app.
