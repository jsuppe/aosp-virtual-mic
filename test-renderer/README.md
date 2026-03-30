# Virtual Mic Test Renderer

A simple test program that generates a sine wave and sends it to the Virtual Mic HAL.

## Building

### Option 1: AOSP Build System

Copy to your AOSP tree and build:
```bash
cp -r test-renderer /path/to/aosp/vendor/test/virtual_mic_renderer
cd /path/to/aosp
m virtual_mic_renderer
adb push out/target/product/*/vendor/bin/virtual_mic_renderer /data/local/tmp/
```

### Option 2: NDK Standalone

```bash
# Set NDK path
export NDK=/path/to/android-ndk

# Compile
$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android30-clang++ \
    -o virtual_mic_renderer \
    virtual_mic_renderer.cpp \
    -llog -static-libstdc++

# Push to device
adb push virtual_mic_renderer /data/local/tmp/
adb shell chmod +x /data/local/tmp/virtual_mic_renderer
```

## Usage

```bash
# Basic usage (440 Hz, 50% amplitude, 10 seconds)
adb shell /data/local/tmp/virtual_mic_renderer

# Custom frequency, amplitude, duration
adb shell /data/local/tmp/virtual_mic_renderer 1000 0.8 30
# Args: frequency(Hz) amplitude(0-1) duration(sec)
```

## Testing

1. Start the renderer:
   ```bash
   adb shell /data/local/tmp/virtual_mic_renderer 440 0.5 60
   ```

2. On the device, open a voice recorder app and record

3. The recorded audio should contain a 440 Hz sine wave

## Expected Output

```
Virtual Mic Test Renderer
Frequency: 440.0 Hz, Amplitude: 0.50, Duration: 10 sec
Connecting to /data/vendor/virtualmic/virtual_mic.sock...
Connected!
Sending ashmem fd to HAL...
Ashmem fd sent!
Generating 10 seconds of 440.0 Hz sine wave...
  1/10 seconds
  2/10 seconds
  ...
  10/10 seconds
Done! Wrote 480000 frames
```

## Troubleshooting

### "Failed to connect"
- Check if HAL is running: `adb shell service list | grep virtualmic`
- Check socket exists: `adb shell ls -la /data/vendor/virtualmic/`
- Check SELinux: `adb shell getenforce` (try `setenforce 0` for testing)

### "Permission denied"
- Run as root: `adb root` then try again
- Check SELinux audit logs: `adb logcat | grep avc`
