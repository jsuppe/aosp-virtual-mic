# Virtual Microphone Test App

Side-by-side audio waveform comparison to prove the virtual microphone pipeline.

## Layout

```
┌─────────────────────────────────────────────────┐
│           Virtual Microphone Test               │
├────────────────────┬────────────────────────────┤
│  🔊 SOURCE AUDIO   │  🎤 VIRTUAL MIC OUTPUT     │
│                    │                            │
│  ╭─────────────╮   │   ╭─────────────╮          │
│  │ ∿∿∿∿∿∿∿∿∿∿ │   │   │ ∿∿∿∿∿∿∿∿∿∿ │          │
│  │ ∿∿∿∿∿∿∿∿∿∿ │   │   │ ∿∿∿∿∿∿∿∿∿∿ │          │
│  ╰─────────────╯   │   ╰─────────────╯          │
│                    │                            │
│  [Sine] [Sweep]    │   Latency: 12ms            │
│  [Chirp] [Noise]   │   Correlation: 99.2%       │
├────────────────────┴────────────────────────────┤
│  440Hz Sine | 48kHz 16-bit | Buffer: 512       │
└─────────────────────────────────────────────────┘
```

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        Test App                              │
│                                                              │
│  ┌─────────────────┐      ┌─────────────────┐               │
│  │ AudioGenerator  │      │  AudioRecorder  │               │
│  │                 │      │                 │               │
│  │ - Sine wave     │      │ - Open virtual  │               │
│  │ - Frequency     │      │   mic (ID 100)  │               │
│  │   sweep         │      │ - Record PCM    │               │
│  │ - White noise   │      │ - Display       │               │
│  │ - Chirp         │      │   waveform      │               │
│  └────────┬────────┘      └────────┬────────┘               │
│           │                        │                         │
│           ▼                        │                         │
│  ┌─────────────────┐              │                         │
│  │ Shared Memory   │              │                         │
│  │ /data/local/tmp │              │                         │
│  │ /virtual_mic_shm│              │                         │
│  └────────┬────────┘              │                         │
│           │                        │                         │
└───────────┼────────────────────────┼─────────────────────────┘
            │                        │
            ▼                        │
┌───────────────────────┐           │
│  Virtual Mic HAL      │           │
│                       │◄──────────┘
│  - Read from shm      │   AudioRecord API
│  - Provide to         │   requests samples
│    AudioFlinger       │
└───────────────────────┘
```

## Audio Test Signals

### 1. Sine Wave (Default)
- Configurable frequency (20Hz - 20kHz)
- Default: 440Hz (A4)
- Clean reference signal

### 2. Frequency Sweep
- Linear sweep from 20Hz to 20kHz
- 5 second duration
- Tests full frequency response

### 3. Chirp
- Logarithmic frequency sweep
- Good for impulse response measurement
- Helps measure latency precisely

### 4. White Noise
- Random samples
- Tests full spectrum
- Good for correlation analysis

### 5. Square Wave
- Tests harmonic content preservation
- Easy to see distortion

## Latency Measurement

The app embeds a timestamp marker in the audio:
1. Source generates audio with embedded timing pulse
2. Records from virtual mic
3. Cross-correlates to find delay
4. Displays latency in milliseconds

## Waveform Display

- Real-time scrolling waveform
- Amplitude visualization
- Peak level meters
- Configurable time scale (10ms - 1s view)

## Audio Parameters

- Sample Rate: 48000 Hz
- Bit Depth: 16-bit PCM
- Channels: Mono (1) or Stereo (2)
- Buffer Size: 512 samples (configurable)

## Building

```bash
cd unified-test
./gradlew assembleDebug
adb install app/build/outputs/apk/debug/app-debug.apk
```

## Success Criteria

- [ ] Source waveform displays correctly
- [ ] Virtual mic waveform matches source
- [ ] Waveforms are visually synchronized
- [ ] Latency < 50ms (ideally < 20ms)
- [ ] No audible artifacts or dropouts
- [ ] Correlation > 95%
