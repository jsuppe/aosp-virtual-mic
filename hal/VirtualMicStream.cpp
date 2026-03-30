/*
 * VirtualMicStream - Audio input stream that reads from shared memory
 */

#define LOG_TAG "VirtualMicStream"

#include "VirtualMicStream.h"
#include "VirtualMicSource.h"

#include <log/log.h>
#include <utils/Timers.h>
#include <cstring>

namespace aidl::android::hardware::audio::virtualmic {

VirtualMicStream::VirtualMicStream(std::shared_ptr<VirtualMicSource> source,
                                   const StreamConfig& config)
    : mSource(source), mConfig(config) {
    ALOGI("VirtualMicStream created: %uHz, %u channels, buffer %zu bytes",
          config.sampleRate, config.channelCount, config.bufferSizeBytes());
}

VirtualMicStream::~VirtualMicStream() {
    stop();
    ALOGI("VirtualMicStream destroyed");
}

int VirtualMicStream::start() {
    std::lock_guard<std::mutex> lock(mLock);
    
    if (mActive) {
        return 0;  // Already started
    }
    
    mStartTimeNs = systemTime(SYSTEM_TIME_MONOTONIC);
    mFramesRead = 0;
    mStandby = false;
    mActive = true;
    
    ALOGI("Stream started");
    return 0;
}

int VirtualMicStream::stop() {
    std::lock_guard<std::mutex> lock(mLock);
    
    mActive = false;
    
    ALOGI("Stream stopped, %llu frames read", (unsigned long long)mFramesRead);
    return 0;
}

int VirtualMicStream::standby() {
    std::lock_guard<std::mutex> lock(mLock);
    
    mStandby = true;
    
    ALOGV("Stream in standby");
    return 0;
}

ssize_t VirtualMicStream::read(void* buffer, size_t bytes) {
    if (!mActive.load(std::memory_order_acquire)) {
        // Not active - fill with silence
        memset(buffer, 0, bytes);
        return bytes;
    }
    
    if (mStandby.load(std::memory_order_acquire)) {
        // In standby - exit standby on first read
        std::lock_guard<std::mutex> lock(mLock);
        mStandby = false;
        mStartTimeNs = systemTime(SYSTEM_TIME_MONOTONIC);
        mFramesRead = 0;
    }
    
    // Read from source
    size_t bytesRead = mSource->read(buffer, bytes);
    
    // Track frames for timing
    size_t framesRead = bytesRead / mConfig.frameSize();
    mFramesRead += framesRead;
    
    // Simulate audio timing - sleep to match real-time audio rate
    // This prevents reading faster than audio would actually be captured
    int64_t expectedTimeNs = mStartTimeNs + 
        (mFramesRead * 1000000000LL / mConfig.sampleRate);
    int64_t currentTimeNs = systemTime(SYSTEM_TIME_MONOTONIC);
    int64_t sleepNs = expectedTimeNs - currentTimeNs;
    
    if (sleepNs > 0 && sleepNs < 100000000LL) {  // Max 100ms sleep
        struct timespec ts;
        ts.tv_sec = sleepNs / 1000000000LL;
        ts.tv_nsec = sleepNs % 1000000000LL;
        nanosleep(&ts, nullptr);
    } else if (sleepNs < -100000000LL) {
        // We're too far behind - reset timing
        mStartTimeNs = currentTimeNs;
        mFramesRead = framesRead;
    }
    
    return bytesRead;
}

uint32_t VirtualMicStream::getLatencyMs() const {
    // Latency = buffer size + estimated shared memory latency
    uint32_t bufferLatencyMs = (mConfig.bufferSizeFrames * 1000) / mConfig.sampleRate;
    uint32_t shmLatencyMs = 5;  // Estimated shared memory latency
    return bufferLatencyMs + shmLatencyMs;
}

}  // namespace aidl::android::hardware::audio::virtualmic
