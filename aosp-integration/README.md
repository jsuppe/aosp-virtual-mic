# AOSP Integration Files

These files integrate the Virtual Mic HAL into the AOSP Audio HAL.

## Installation

Copy these files to your AOSP tree:

### Audio HAL Core Implementation
```
hardware/interfaces/audio/aidl/default/
├── include/core-impl/
│   ├── ModuleVirtualMic.h
│   └── StreamVirtualMic.h
└── virtualmic/
    ├── ModuleVirtualMic.cpp
    ├── StreamVirtualMic.cpp
    ├── VirtualMicSource.cpp
    ├── VirtualMicSource.h
    ├── VirtualMicSocket.cpp
    ├── VirtualMicSocket.h
    └── AudioBufferHeader.h
```

### Audio Policy Configuration
```
frameworks/av/services/audiopolicy/config/
└── virtualmic_audio_policy_configuration.xml
```

## Required AOSP Modifications

### 1. Module.h - Add type enum
```cpp
enum Type : int { DEFAULT, R_SUBMIX, STUB, USB, BLUETOOTH, VIRTUALMIC };
```

### 2. Module.cpp - Add handling
- Include: `#include "core-impl/ModuleVirtualMic.h"`
- createInstance(): Add `case Type::VIRTUALMIC:`
- typeFromString(): Add `else if (type == "virtualmic")`
- operator<<(): Add `case Module::Type::VIRTUALMIC:`

### 3. Configuration.cpp - Add config case
```cpp
case Module::Type::VIRTUALMIC:
    return getRSubmixConfiguration();  // Uses same config for now
```

### 4. Android.bp - Add source files
```
"virtualmic/ModuleVirtualMic.cpp",
"virtualmic/StreamVirtualMic.cpp",
"virtualmic/VirtualMicSource.cpp",
"virtualmic/VirtualMicSocket.cpp",
```

### 5. frameworks/av Android.bp - Add prebuilt_etc and filegroup

### 6. device.mk - Add PRODUCT_COPY_FILES entry

## Socket Path

The HAL listens on: `/data/local/tmp/virtual_mic.sock`

Audio data is passed via ashmem (fd sent over socket with SCM_RIGHTS).
