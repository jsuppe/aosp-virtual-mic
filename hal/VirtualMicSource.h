/*
 * VirtualMicSource - Reads audio from renderer via shared memory
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

#include "AudioBufferHeader.h"

namespace aidl::android::hardware::audio::virtualmic {

class VirtualMicSocket;

class VirtualMicSource {
public:
    VirtualMicSource();
    ~VirtualMicSource();
    
    // Start/stop the socket server
    bool start();
    void stop();
    
    // Read audio samples from the ring buffer
    // Returns number of bytes read (may be less than requested)
    size_t read(void* buffer, size_t bytes);
    
    // Check if renderer is connected
    bool isRendererConnected() const;
    
    // Get audio configuration
    uint32_t getSampleRate() const;
    uint32_t getChannelCount() const;
    AudioFormat getFormat() const;
    
    // Called by socket when renderer connects and sends fd
    void onRendererConnected(int fd, size_t size);
    
private:
    std::unique_ptr<VirtualMicSocket> mSocket;
    
    // Shared memory
    void* mMappedMemory = nullptr;
    size_t mMappedSize = 0;
    int mShmFd = -1;
    
    AudioBufferHeader* mHeader = nullptr;
    uint8_t* mRingBuffer = nullptr;
    
    std::atomic<bool> mRendererConnected{false};
    mutable std::mutex mLock;
    
    // For generating silence when no renderer
    // uint64_t mSilenceSamplesGenerated = 0;  // unused
};

}  // namespace aidl::android::hardware::audio::virtualmic
