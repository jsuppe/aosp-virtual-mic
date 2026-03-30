#pragma once

#include <cstdint>
#include <atomic>

namespace vmic {

// Shared memory header for audio - must match HAL's AudioHeader
struct AudioHeader {
    std::atomic<uint32_t> magic;          // 0x564D4943 = "VMIC"
    std::atomic<uint32_t> version;
    std::atomic<uint32_t> sampleRate;
    std::atomic<uint32_t> channels;
    std::atomic<uint32_t> format;         // PCM format
    std::atomic<uint32_t> bufferSize;     // Samples per buffer
    std::atomic<uint64_t> frameNumber;
    std::atomic<uint64_t> timestamp;
    std::atomic<uint32_t> dataOffset;
    std::atomic<uint32_t> dataSize;
    std::atomic<uint32_t> flags;
    
    // Ring buffer indices
    std::atomic<uint32_t> writeIndex;
    std::atomic<uint32_t> readIndex;
};

static constexpr uint32_t VMIC_MAGIC = 0x564D4943;  // "VMIC"
static constexpr uint32_t VMIC_VERSION = 1;
static constexpr uint32_t FLAG_NEW_DATA = 1;
static constexpr uint32_t FLAG_RENDERER_ACTIVE = 2;
static constexpr uint32_t FORMAT_PCM_16BIT = 1;

class VirtualMicWriter {
public:
    VirtualMicWriter();
    ~VirtualMicWriter();
    
    bool initialize(uint32_t sampleRate, uint32_t channels, uint32_t bufferSize);
    void shutdown();
    void writeSamples(const int16_t* samples, uint32_t count);
    
    uint32_t getSampleRate() const { return mSampleRate; }
    uint32_t getChannels() const { return mChannels; }
    bool isInitialized() const { return mMappedAddr != nullptr; }
    
private:
    int mFd = -1;
    void* mMappedAddr = nullptr;
    size_t mMappedSize = 0;
    AudioHeader* mHeader = nullptr;
    int16_t* mAudioData = nullptr;
    
    uint32_t mSampleRate = 0;
    uint32_t mChannels = 0;
    uint32_t mBufferSize = 0;
    uint32_t mRingBufferSize = 0;
    uint64_t mFrameNumber = 0;
};

}  // namespace vmic
