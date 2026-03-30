/*
 * VirtualMicStream - Audio input stream that reads from shared memory
 *
 * This is a simplified stream implementation for the virtual microphone.
 * It reads PCM samples from VirtualMicSource and provides them to AudioFlinger.
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>

namespace aidl::android::hardware::audio::virtualmic {

class VirtualMicSource;

/**
 * Audio stream configuration
 */
struct StreamConfig {
    uint32_t sampleRate = 48000;
    uint32_t channelCount = 1;
    uint32_t bytesPerSample = 2;  // 16-bit PCM
    uint32_t bufferSizeFrames = 480;  // 10ms at 48kHz
    
    size_t frameSize() const { return channelCount * bytesPerSample; }
    size_t bufferSizeBytes() const { return bufferSizeFrames * frameSize(); }
};

/**
 * VirtualMicStream - Provides audio input from shared memory
 *
 * This stream is opened by AudioFlinger when an app requests microphone access.
 * It reads samples from VirtualMicSource (which gets them from the renderer app).
 */
class VirtualMicStream {
public:
    explicit VirtualMicStream(std::shared_ptr<VirtualMicSource> source,
                              const StreamConfig& config);
    ~VirtualMicStream();
    
    // Stream lifecycle
    int start();
    int stop();
    int standby();
    
    // Read audio data - called by AudioFlinger
    // Fills buffer with PCM samples, returns bytes read
    ssize_t read(void* buffer, size_t bytes);
    
    // Get stream parameters
    uint32_t getSampleRate() const { return mConfig.sampleRate; }
    uint32_t getChannelCount() const { return mConfig.channelCount; }
    size_t getBufferSize() const { return mConfig.bufferSizeBytes(); }
    
    // Get latency in milliseconds
    uint32_t getLatencyMs() const;
    
private:
    std::shared_ptr<VirtualMicSource> mSource;
    StreamConfig mConfig;
    
    std::atomic<bool> mActive{false};
    std::atomic<bool> mStandby{true};
    
    // Timing for simulated audio clock
    int64_t mStartTimeNs = 0;
    uint64_t mFramesRead = 0;
    
    mutable std::mutex mLock;
};

}  // namespace aidl::android::hardware::audio::virtualmic
