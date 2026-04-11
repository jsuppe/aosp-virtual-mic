# Virtual Microphone HAL

Audio input HAL that receives PCM samples from renderer apps via a
shared-memory ring buffer. The HAL is split into a version-independent
core and one adapter per supported Android Audio HAL version so a single
pipeline implementation can front multiple Android releases.

## Directory layout

```
hal/
  core/         # Pure C++ shared pipeline (no AIDL/HIDL deps)
  aidl-v2/      # AIDL Audio Core HAL V2 adapter (Android 14+)
  hidl-v7/      # HIDL Audio HAL 7.x adapter (Android 13 and older)
```

Each subdirectory carries its own `Android.bp`. The top-level
`hal/Android.bp` is intentionally empty — Soong discovers subdirectories
automatically.

### `core/`

- `AudioBufferHeader.h` — Shared-memory layout and ring-buffer helpers.
- `VirtualMicSource.{h,cpp}` — Maps the renderer's ashmem and reads PCM.
- `VirtualMicSocket.{h,cpp}` — Unix domain socket server that receives
  the ashmem fd via `SCM_RIGHTS`.
- `Android.bp` — Builds `virtual-mic-core` (`cc_library_static`).

Core code lives in the plain `virtualmic` namespace and takes/returns
plain C++ types. It has no dependency on any Android HAL interface.

### `aidl-v2/`

Adapter for the AIDL Audio Core HAL V2 (`android.hardware.audio.core`
version 2, introduced in Android 14).

- `include/core-impl/ModuleVirtualMic.h`
- `include/core-impl/StreamVirtualMic.h`
- `ModuleVirtualMic.cpp` — `Module` subclass that owns a
  `virtualmic::VirtualMicSource` and exposes it as `IModule/virtualmic`.
- `StreamVirtualMic.cpp` — `StreamCommonImpl` / `StreamIn` subclass that
  pulls PCM frames from the core source inside `transfer()`.
- `service.cpp` — Optional standalone service entry point. In the
  primary deployment path `ModuleVirtualMic` is loaded by AOSP's
  existing `android.hardware.audio.service` binary via
  `Module::createInstance(Type::VIRTUALMIC)`; this file lets the HAL
  also be built as its own process for isolated testing.
- `android.hardware.audio.service.virtualmic.rc`
- `android.hardware.audio.service.virtualmic.xml` — VINTF fragment.
- `Android.bp` — Builds `android.hardware.audio.virtualmic-impl`
  (`cc_library_shared`) and `android.hardware.audio.service.virtualmic`
  (`cc_binary`), both statically linking `virtual-mic-core`.

The adapter consumes core headers via `include_dirs:
["hardware/interfaces/audio/virtualmic/core"]` because Soong disallows
`..` in `local_include_dirs`.

### `hidl-v7/`

Adapter for the HIDL Audio HAL 7.x. Maintained alongside `aidl-v2/` for
devices that have not yet migrated to the AIDL HAL. See that directory
for details.

## Shared memory protocol

1. Renderer creates ashmem with `ASharedMemory_create()`.
2. Renderer connects to `/data/vendor/virtualmic/virtual_mic.sock`.
3. Renderer sends the fd via `SCM_RIGHTS`, then sends the buffer size as
   a `uint64_t`.
4. HAL maps the region and validates `AudioBufferHeader.magic/version`.
5. Renderer writes PCM into the ring buffer; HAL reads and forwards to
   AudioFlinger through the version-specific stream adapter.

See `core/AudioBufferHeader.h` for the exact layout.

## Audio format

- Sample rate: configurable (default 48000 Hz)
- Channels: mono (1) or stereo (2)
- Format: 16-bit signed PCM (`PCM_16_BIT`) or float (`PCM_FLOAT`)
- Transport: ring buffer with `writePos` / `readPos` atomics
