/*
 * AudioBufferHeader - Shared memory layout for virtual microphone
 *
 * Similar to FrameHeader for camera, but designed for audio streaming.
 * Uses a ring buffer for continuous audio flow.
 */

#pragma once

#include <cstdint>
#include <atomic>

namespace aidl::android::hardware::audio::virtualmic {

// Magic number: "VMIC" in little-endian
constexpr uint32_t AUDIO_BUFFER_MAGIC = 0x43494D56;

// Header version
constexpr uint32_t AUDIO_BUFFER_VERSION = 1;

// Audio format constants
enum class AudioFormat : uint32_t {
    PCM_16_BIT = 1,
    PCM_FLOAT = 2,
};

/**
 * Shared memory layout:
 *
 * ┌─────────────────────────────────────────────────────────────┐
 * │ AudioBufferHeader (64 bytes)                                │
 * ├─────────────────────────────────────────────────────────────┤
 * │ Ring Buffer (configurable size)                             │
 * │ ┌─────────────────────────────────────────────────────────┐ │
 * │ │ Audio samples (PCM data)                                │ │
 * │ │                                                         │ │
 * │ │   writePos ────►  ▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░░                │ │
 * │ │                   ◄─── readPos                          │ │
 * │ └─────────────────────────────────────────────────────────┘ │
 * └─────────────────────────────────────────────────────────────┘
 */

struct AudioBufferHeader {
    // Identification
    uint32_t magic;          // Must be AUDIO_BUFFER_MAGIC
    uint32_t version;        // Header version
    
    // Audio configuration (set by renderer)
    uint32_t sampleRate;     // e.g., 48000
    uint32_t channelCount;   // 1 = mono, 2 = stereo
    AudioFormat format;      // PCM format
    uint32_t bytesPerSample; // e.g., 2 for 16-bit, 4 for float
    
    // Ring buffer configuration
    uint32_t ringBufferOffset;  // Offset from header start to ring buffer
    uint32_t ringBufferSize;    // Total ring buffer size in bytes
    
    // Ring buffer state (atomics for lock-free operation)
    std::atomic<uint32_t> writePos;  // Write position (renderer updates)
    std::atomic<uint32_t> readPos;   // Read position (HAL updates)
    
    // Statistics
    std::atomic<uint64_t> totalSamplesWritten;
    std::atomic<uint64_t> totalSamplesRead;
    
    // Flags
    std::atomic<uint32_t> flags;
    
    // Padding to 64 bytes
    uint32_t reserved[2];
    
    // Flag definitions
    static constexpr uint32_t FLAG_RENDERER_CONNECTED = 0x01;
    static constexpr uint32_t FLAG_ACTIVE = 0x02;
    
    // Helper methods
    bool isValid() const {
        return magic == AUDIO_BUFFER_MAGIC && version == AUDIO_BUFFER_VERSION;
    }
    
    uint32_t frameSize() const {
        return channelCount * bytesPerSample;
    }
    
    uint32_t availableToRead() const {
        uint32_t wp = writePos.load(std::memory_order_acquire);
        uint32_t rp = readPos.load(std::memory_order_relaxed);
        if (wp >= rp) {
            return wp - rp;
        } else {
            return ringBufferSize - rp + wp;
        }
    }
    
    uint32_t availableToWrite() const {
        return ringBufferSize - availableToRead() - 1;  // -1 to distinguish full from empty
    }
};

static_assert(sizeof(AudioBufferHeader) == 64, "AudioBufferHeader must be 64 bytes");

}  // namespace aidl::android::hardware::audio::virtualmic
