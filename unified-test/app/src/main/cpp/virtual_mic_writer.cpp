/**
 * VirtualMicWriter - Write audio samples to shared memory for HAL
 */

#include "virtual_mic_writer.h"
#include <android/log.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <chrono>

#define LOG_TAG "VMicWriter"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace vmic {

VirtualMicWriter::VirtualMicWriter() = default;

VirtualMicWriter::~VirtualMicWriter() {
    shutdown();
}

bool VirtualMicWriter::initialize(uint32_t sampleRate, uint32_t channels, uint32_t bufferSize) {
    if (mMappedAddr != nullptr) {
        LOGE("Already initialized");
        return false;
    }
    
    mSampleRate = sampleRate;
    mChannels = channels;
    mBufferSize = bufferSize;
    
    // Ring buffer: 10x the buffer size for latency tolerance
    mRingBufferSize = bufferSize * 10;
    
    // Calculate required size: header + ring buffer of PCM data
    size_t dataOffset = sizeof(AudioHeader);
    dataOffset = (dataOffset + 4095) & ~4095;  // Align to 4KB
    
    size_t dataSize = mRingBufferSize * channels * sizeof(int16_t);
    mMappedSize = dataOffset + dataSize;
    
    // Create shared memory file
    const char* path = "/data/local/tmp/virtual_mic_shm";
    
    mFd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (mFd < 0) {
        LOGE("Failed to create shared memory: %s", strerror(errno));
        return false;
    }
    
    if (ftruncate(mFd, mMappedSize) < 0) {
        LOGE("Failed to set size: %s", strerror(errno));
        close(mFd);
        mFd = -1;
        return false;
    }
    
    mMappedAddr = mmap(nullptr, mMappedSize, PROT_READ | PROT_WRITE, MAP_SHARED, mFd, 0);
    if (mMappedAddr == MAP_FAILED) {
        LOGE("Failed to mmap: %s", strerror(errno));
        mMappedAddr = nullptr;
        close(mFd);
        mFd = -1;
        return false;
    }
    
    // Initialize header
    mHeader = static_cast<AudioHeader*>(mMappedAddr);
    mHeader->magic.store(VMIC_MAGIC, std::memory_order_release);
    mHeader->version.store(VMIC_VERSION, std::memory_order_release);
    mHeader->sampleRate.store(sampleRate, std::memory_order_release);
    mHeader->channels.store(channels, std::memory_order_release);
    mHeader->format.store(FORMAT_PCM_16BIT, std::memory_order_release);
    mHeader->bufferSize.store(bufferSize, std::memory_order_release);
    mHeader->frameNumber.store(0, std::memory_order_release);
    mHeader->timestamp.store(0, std::memory_order_release);
    mHeader->dataOffset.store(dataOffset, std::memory_order_release);
    mHeader->dataSize.store(dataSize, std::memory_order_release);
    mHeader->flags.store(FLAG_RENDERER_ACTIVE, std::memory_order_release);
    mHeader->writeIndex.store(0, std::memory_order_release);
    mHeader->readIndex.store(0, std::memory_order_release);
    
    mAudioData = reinterpret_cast<int16_t*>(static_cast<uint8_t*>(mMappedAddr) + dataOffset);
    memset(mAudioData, 0, dataSize);
    
    LOGI("Initialized: %u Hz, %u ch, buffer %u, ring %u at %s", 
         sampleRate, channels, bufferSize, mRingBufferSize, path);
    return true;
}

void VirtualMicWriter::shutdown() {
    if (mHeader != nullptr) {
        mHeader->flags.fetch_and(~FLAG_RENDERER_ACTIVE, std::memory_order_release);
    }
    
    if (mMappedAddr != nullptr) {
        munmap(mMappedAddr, mMappedSize);
        mMappedAddr = nullptr;
        mHeader = nullptr;
        mAudioData = nullptr;
    }
    
    if (mFd >= 0) {
        close(mFd);
        mFd = -1;
    }
    
    LOGI("Shutdown complete");
}

void VirtualMicWriter::writeSamples(const int16_t* samples, uint32_t count) {
    if (mAudioData == nullptr || mHeader == nullptr) {
        return;
    }
    
    uint32_t writeIdx = mHeader->writeIndex.load(std::memory_order_acquire);
    
    // Write samples to ring buffer
    for (uint32_t i = 0; i < count; i++) {
        uint32_t idx = (writeIdx + i) % mRingBufferSize;
        mAudioData[idx] = samples[i];
    }
    
    // Update write index
    uint32_t newWriteIdx = (writeIdx + count) % mRingBufferSize;
    mHeader->writeIndex.store(newWriteIdx, std::memory_order_release);
    
    // Update timestamp
    auto now = std::chrono::steady_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();
    
    mFrameNumber++;
    mHeader->frameNumber.store(mFrameNumber, std::memory_order_release);
    mHeader->timestamp.store(static_cast<uint64_t>(ns), std::memory_order_release);
    mHeader->flags.fetch_or(FLAG_NEW_DATA, std::memory_order_release);
}

}  // namespace vmic
