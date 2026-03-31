# AOSP Virtual Microphone HAL

A custom Audio HAL module that allows Android apps to inject audio as microphone input via shared memory (ashmem).

## Architecture

```
┌─────────────────┐     Unix Socket      ┌─────────────────┐
│  Renderer App   │ ──────────────────▶  │  Audio HAL      │
│  (generates     │   SCM_RIGHTS fd      │  (VirtualMic    │
│   audio)        │                      │   Module)       │
└────────┬────────┘                      └────────┬────────┘
         │                                        │
         │  Writes to ashmem                      │  Reads from ashmem
         ▼                                        ▼
┌─────────────────────────────────────────────────────────────┐
│                    Shared Memory (ashmem)                    │
│  ┌──────────────────┬───────────────────────────────────┐   │
│  │ AudioBufferHeader │ Ring Buffer (PCM audio samples)   │   │
│  │ (64 bytes)        │ (configurable size)               │   │
│  └──────────────────┴───────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
         │
         │  AudioRecord API
         ▼
┌─────────────────┐
│  Android App    │
│  (records from  │
│   MIC source)   │
└─────────────────┘
```

## Components

### hal/
Audio HAL implementation:
- `ModuleVirtualMic.cpp` - HAL module that registers as "virtualmic"
- `StreamVirtualMic.cpp` - Input stream implementation
- `VirtualMicSource.cpp` - Reads audio from shared memory
- `VirtualMicSocket.cpp` - Unix socket server for renderer connections
- `AudioBufferHeader.h` - Shared memory layout definition

### renderer-test/
- `virtual_mic_renderer.cpp` - Test renderer that generates sine waves

### unified-test/
Android test app that:
- Records from MIC source (routed to virtualmic)
- Displays detected frequency, RMS, peak amplitude
- Shows real-time waveform visualization

### sepolicy/
- `hal_virtualmic.te` - SELinux policy for the HAL and socket

### init.virtualmic.rc
Init script to create the socket directory at boot.

## Building

### HAL (in AOSP tree)
```bash
# Copy hal/ to hardware/interfaces/audio/aidl/default/virtualmic/
# Add to Module.cpp and Android.bp
m com.android.hardware.audio
```

### Test Renderer
```bash
# Copy to vendor/test/virtual_mic_renderer/
m virtual_mic_renderer
adb push out/.../virtual_mic_renderer /data/local/tmp/
```

### Test App
```bash
cd unified-test
./gradlew assembleDebug
adb install app/build/outputs/apk/debug/app-debug.apk
```

## Testing

1. Boot AOSP with the virtualmic module
2. Disable built-in mic in audio policy (or let audio policy route to virtualmic)
3. Start the renderer:
   ```bash
   adb shell /data/local/tmp/virtual_mic_renderer 440 0.5 60
   # Args: frequency(Hz) amplitude(0-1) duration(sec)
   ```
4. Open the test app and tap Record
5. Should see 440 Hz detected with waveform visualization

## Audio Format

- Sample rate: 48000 Hz
- Channels: 2 (stereo)
- Format: PCM 16-bit signed
- Ring buffer: 16384 bytes (4096 stereo frames)

## Ring Buffer Protocol

The renderer and HAL communicate via atomic positions in the shared memory header:

- `writePos` - Byte offset where renderer will write next (updated by renderer)
- `readPos` - Byte offset where HAL will read next (updated by HAL)
- Available to read = `(writePos - readPos) mod ringBufferSize`
- Available to write = `ringBufferSize - availableToRead - 1`

Both positions are **byte offsets** into the ring buffer, not frame indices.

## SELinux

The HAL creates a socket at `/data/vendor/virtualmic/virtual_mic.sock`.
Required policy allows:
- HAL to create directory and socket
- Apps to connect to the socket
- Apps to send file descriptors via SCM_RIGHTS

## License

Apache 2.0 (AOSP compatible)
