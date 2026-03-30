# AOSP Virtual Mic HAL Integration

This directory contains all files needed to integrate the Virtual Mic HAL into AOSP.

## What It Does

The Virtual Mic HAL allows Android apps to inject audio as microphone input via shared memory IPC. A renderer app writes PCM audio to an ashmem buffer, and apps using AudioRecord receive it as mic input.

## Files

### Audio HAL Core (copy to `hardware/interfaces/audio/aidl/default/`)

```
include/core-impl/
├── ModuleVirtualMic.h      # Module class declaration
└── StreamVirtualMic.h      # Stream class declaration

virtualmic/
├── ModuleVirtualMic.cpp    # Module implementation
├── StreamVirtualMic.cpp    # Stream implementation
├── VirtualMicSource.cpp    # Shared memory reader
├── VirtualMicSource.h
├── VirtualMicSocket.cpp    # Unix socket server
├── VirtualMicSocket.h
└── AudioBufferHeader.h     # Shared memory header format
```

### Audio Policy (copy to `frameworks/av/services/audiopolicy/config/`)
- `virtualmic_audio_policy_configuration.xml`

### VINTF Manifest (merge into existing)
- `android.hardware.audio.service-aidl.xml` - Add virtualmic IModule entry

### SELinux Policies (copy to device sepolicy)
- `sepolicy/hal_virtualmic.te` - Type definitions and permissions
- `sepolicy/file_contexts.fragment` - Socket path labeling
- `sepolicy/service_contexts.fragment` - Service registration

## Required AOSP Modifications

### 1. Module.h - Add type enum
```cpp
enum Type : int { DEFAULT, R_SUBMIX, STUB, USB, BLUETOOTH, VIRTUALMIC };
```

### 2. Module.cpp - Add handling
```cpp
#include "core-impl/ModuleVirtualMic.h"

// In createInstance():
case Type::VIRTUALMIC:
    return ndk::SharedRefBase::make<ModuleVirtualMic>(std::move(config));

// In typeFromString():
else if (type == "virtualmic")
    return Module::Type::VIRTUALMIC;

// In operator<<():
case Module::Type::VIRTUALMIC:
    os << "virtualmic";
    break;
```

### 3. Configuration.cpp - Add config case
```cpp
case Module::Type::VIRTUALMIC:
    return getRSubmixConfiguration();  // Uses r_submix config
```

### 4. Android.bp - Add source files
```
"virtualmic/ModuleVirtualMic.cpp",
"virtualmic/StreamVirtualMic.cpp",
"virtualmic/VirtualMicSource.cpp",
"virtualmic/VirtualMicSocket.cpp",
```

### 5. Audio policy Android.bp - Add prebuilt_etc and filegroup

### 6. device.mk - Add PRODUCT_COPY_FILES for the XML config

### 7. VINTF manifest - Add IModule/virtualmic entry

### 8. SELinux - Add policy files and contexts

## Socket Path

The HAL listens on: `/data/vendor/virtualmic/virtual_mic.sock`

Audio data format:
- 48kHz stereo 16-bit PCM
- Passed via ashmem (fd sent over socket with SCM_RIGHTS)
- Ring buffer with AudioBufferHeader for synchronization

## Testing

After building and booting:
```bash
# Verify module is registered
adb shell service list | grep virtualmic

# Check socket exists
adb shell ls -la /data/vendor/virtualmic/

# View logs
adb logcat | grep -i virtualmic
```

## Architecture

```
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────┐
│  Renderer App   │────▶│  VirtualMicSocket │────▶│ VirtualMicSource│
│  (writes audio) │     │  (Unix socket)    │     │ (reads ashmem)  │
└─────────────────┘     └──────────────────┘     └────────┬────────┘
                                                          │
                                                          ▼
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────┐
│   AudioRecord   │◀────│  AudioFlinger    │◀────│ StreamVirtualMic│
│   (app reads)   │     │                  │     │ (HAL stream)    │
└─────────────────┘     └──────────────────┘     └─────────────────┘
```
