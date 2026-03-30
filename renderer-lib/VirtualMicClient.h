/*
 * VirtualMicClient - Client library for apps to inject audio into virtual mic
 */

#pragma once

#include <cstdint>

namespace vmic {

class VirtualMicClient {
public:
    VirtualMicClient();
    ~VirtualMicClient();
    
    /**
     * Initialize the client with audio configuration.
     * Creates shared memory and connects to the HAL socket.
     * 
     * @param sampleRate Sample rate in Hz (e.g., 48000)
     * @param channelCount Number of channels (1 = mono, 2 = stereo)
     * @param bufferMs Ring buffer size in milliseconds
     * @return true if successful
     */
    bool initialize(uint32_t sampleRate, uint32_t channelCount, uint32_t bufferMs = 100);
    
    /**
     * Write audio samples to the ring buffer.
     * 
     * @param samples Pointer to PCM samples (16-bit signed)
     * @param sampleCount Number of samples (per channel)
     * @return Number of samples actually written
     */
    size_t writeSamples(const int16_t* samples, size_t sampleCount);
    
    /**
     * Get available space in ring buffer.
     * 
     * @return Number of samples that can be written
     */
    size_t availableToWrite() const;
    
    /**
     * Shutdown and cleanup.
     */
    void shutdown();
    
    // Getters
    uint32_t getSampleRate() const { return mSampleRate; }
    uint32_t getChannelCount() const { return mChannelCount; }
    bool isConnected() const { return mConnected; }
    
private:
    bool connectToHal();
    bool sendSharedMemoryFd();
    
    uint32_t mSampleRate = 0;
    uint32_t mChannelCount = 0;
    uint32_t mBufferMs = 0;
    
    int mShmFd = -1;
    void* mMappedMemory = nullptr;
    size_t mMappedSize = 0;
    
    bool mConnected = false;
    
    static constexpr const char* SOCKET_PATH = "/data/local/tmp/virtual_mic.sock";
};

}  // namespace vmic
