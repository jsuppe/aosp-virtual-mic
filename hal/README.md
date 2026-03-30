# Virtual Microphone HAL

Audio input HAL that receives PCM samples from renderer apps via shared memory.

## Architecture

```
┌──────────────────────┐        ┌────────────────────────────┐
│   Renderer App       │        │    Virtual Mic HAL         │
│                      │        │                            │
│  VirtualMicClient    │        │  VirtualMicSocket          │
│  - Create ashmem     │───────►│  - Accept connections      │
│  - Connect socket    │ socket │  - Receive ashmem fd       │
│  - Write PCM samples │        │                            │
│                      │        │  VirtualMicSource          │
│  ┌────────────────┐  │        │  - mmap shared memory      │
│  │ Ring Buffer    │◄─┼────────┼──│ - Read PCM samples       │
│  │ (ashmem)       │  │        │                            │
│  └────────────────┘  │        │  VirtualMicStream          │
└──────────────────────┘        │  - Provide to AudioFlinger │
                                └────────────────────────────┘
```

## Shared Memory Layout

```
┌─────────────────────────────────────────────────┐
│ AudioBufferHeader (64 bytes)                    │
│ - magic: 0x43494D56 ("VMIC")                    │
│ - sampleRate, channelCount, format              │
│ - ringBufferOffset, ringBufferSize              │
│ - writePos (atomic), readPos (atomic)           │
├─────────────────────────────────────────────────┤
│ Ring Buffer (configurable size)                 │
│ ┌─────────────────────────────────────────────┐ │
│ │ PCM samples (16-bit signed)                 │ │
│ │                                             │ │
│ │ writePos ──►  ▓▓▓▓▓▓▓░░░░░░░░░░             │ │
│ │               ◄── readPos                   │ │
│ └─────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────┘
```

## Files

### HAL Components
- `AudioBufferHeader.h` - Shared memory layout definition
- `VirtualMicSource.h/.cpp` - Reads audio from shared memory
- `VirtualMicSocket.h/.cpp` - Unix socket server for fd passing
- `VirtualMicModule.cpp` - Audio HAL module (TODO)
- `VirtualMicStream.cpp` - Audio input stream (TODO)
- `service.cpp` - HAL service entry point (TODO)

### Renderer Library
- `renderer-lib/VirtualMicClient.h/.cpp` - Client for injecting audio

### Build
- `Android.bp` - Build configuration

## Socket Protocol

1. Renderer creates ashmem with `ASharedMemory_create()`
2. Renderer connects to `/data/local/tmp/virtual_mic.sock`
3. Renderer sends fd via `SCM_RIGHTS`
4. Renderer sends buffer size as uint64_t
5. HAL maps the shared memory read-only
6. Renderer writes PCM samples to ring buffer
7. HAL reads samples and provides to AudioFlinger

## Audio Format

- Sample Rate: Configurable (default 48000 Hz)
- Channels: Mono (1) or Stereo (2)
- Format: 16-bit signed PCM
- Buffer: Ring buffer with configurable size

## Status

- [x] AudioBufferHeader - Shared memory layout
- [x] VirtualMicSource - Read from shared memory
- [x] VirtualMicSocket - Socket server
- [x] VirtualMicClient - Renderer library
- [ ] VirtualMicModule - Audio HAL module
- [ ] VirtualMicStream - Audio input stream
- [ ] service.cpp - HAL service
- [ ] init.rc, VINTF manifest
- [ ] Build integration
- [ ] Test app
