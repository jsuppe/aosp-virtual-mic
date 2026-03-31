/*
 * VirtualMicSource - Reads audio from renderer via shared memory
 */

#define LOG_TAG "VirtualMicSource"

#include "VirtualMicSource.h"
#include "VirtualMicSocket.h"

#include <log/log.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>

namespace aidl::android::hardware::audio::virtualmic {

VirtualMicSource::VirtualMicSource() {
    ALOGI("VirtualMicSource created");
}

VirtualMicSource::~VirtualMicSource() {
    stop();
    
    if (mMappedMemory != nullptr && mMappedMemory != MAP_FAILED) {
        munmap(mMappedMemory, mMappedSize);
    }
    if (mShmFd >= 0) {
        close(mShmFd);
    }
    
    ALOGI("VirtualMicSource destroyed");
}

bool VirtualMicSource::start() {
    std::lock_guard<std::mutex> lock(mLock);
    
    mSocket = std::make_unique<VirtualMicSocket>(this);
    if (!mSocket->start()) {
        ALOGE("Failed to start socket server");
        mSocket.reset();
        return false;
    }
    
    ALOGI("VirtualMicSource started, waiting for renderer...");
    return true;
}

void VirtualMicSource::stop() {
    std::lock_guard<std::mutex> lock(mLock);
    
    if (mSocket) {
        mSocket->stop();
        mSocket.reset();
    }
    
    mRendererConnected.store(false, std::memory_order_release);
}

void VirtualMicSource::onRendererConnected(int fd, size_t size) {
    std::lock_guard<std::mutex> lock(mLock);
    
    ALOGI("Renderer connected, fd=%d, size=%zu", fd, size);
    
    // Clean up old mapping if any
    if (mMappedMemory != nullptr && mMappedMemory != MAP_FAILED) {
        munmap(mMappedMemory, mMappedSize);
    }
    if (mShmFd >= 0) {
        close(mShmFd);
    }
    
    // Map the new shared memory (read-write for HAL to update read position)
    mShmFd = fd;
    mMappedSize = size;
    mMappedMemory = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    
    if (mMappedMemory == MAP_FAILED) {
        ALOGE("Failed to mmap shared memory: %s", strerror(errno));
        mMappedMemory = nullptr;
        close(mShmFd);
        mShmFd = -1;
        return;
    }
    
    // Set up pointers
    mHeader = static_cast<AudioBufferHeader*>(mMappedMemory);
    
    ALOGI("Header check: sizeof(AudioBufferHeader)=%zu, magic=0x%08X (expected 0x%08X), version=%u (expected %u)",
          sizeof(AudioBufferHeader), mHeader->magic, AUDIO_BUFFER_MAGIC, 
          mHeader->version, AUDIO_BUFFER_VERSION);
    
    if (!mHeader->isValid()) {
        ALOGE("Invalid audio buffer header - magic or version mismatch");
        munmap(mMappedMemory, mMappedSize);
        mMappedMemory = nullptr;
        mHeader = nullptr;
        close(mShmFd);
        mShmFd = -1;
        return;
    }
    
    mRingBuffer = static_cast<uint8_t*>(mMappedMemory) + mHeader->ringBufferOffset;
    
    ALOGI("Mapped audio buffer: %uHz, %u channels, ring buffer %u bytes",
          mHeader->sampleRate, mHeader->channelCount, mHeader->ringBufferSize);
    
    mRendererConnected.store(true, std::memory_order_release);
}

size_t VirtualMicSource::read(void* buffer, size_t bytes) {
    // Hold lock for entire read to prevent race with disconnect
    std::lock_guard<std::mutex> lock(mLock);
    
    // Very defensive checks - if ANYTHING is not perfectly ready, return silence
    if (!mRendererConnected.load(std::memory_order_acquire)) {
        memset(buffer, 0, bytes);
        return bytes;
    }
    
    if (mMappedMemory == nullptr || mMappedMemory == MAP_FAILED ||
        mHeader == nullptr || mRingBuffer == nullptr) {
        ALOGW("read: invalid memory state, returning silence");
        memset(buffer, 0, bytes);
        return bytes;
    }
    
    // Validate header before accessing its members
    if (mHeader->magic != AUDIO_BUFFER_MAGIC) {
        ALOGW("read: header magic invalid (0x%08X), returning silence", mHeader->magic);
        memset(buffer, 0, bytes);
        return bytes;
    }
    
    // Read from ring buffer
    uint32_t available = mHeader->availableToRead();
    size_t toRead = std::min(bytes, static_cast<size_t>(available));
    
    if (toRead == 0) {
        // Buffer underrun - generate silence
        memset(buffer, 0, bytes);
        return bytes;
    }
    
    uint32_t readPos = mHeader->readPos.load(std::memory_order_relaxed);
    uint8_t* dst = static_cast<uint8_t*>(buffer);
    
    // Handle wrap-around
    uint32_t toEnd = mHeader->ringBufferSize - readPos;
    if (toRead <= toEnd) {
        memcpy(dst, mRingBuffer + readPos, toRead);
        readPos += toRead;
        if (readPos >= mHeader->ringBufferSize) {
            readPos = 0;
        }
    } else {
        // Wrap around
        memcpy(dst, mRingBuffer + readPos, toEnd);
        memcpy(dst + toEnd, mRingBuffer, toRead - toEnd);
        readPos = toRead - toEnd;
    }
    
    mHeader->readPos.store(readPos, std::memory_order_release);
    mHeader->totalSamplesRead.fetch_add(toRead / mHeader->bytesPerSample, 
                                        std::memory_order_relaxed);
    
    // If we didn't read enough, pad with silence
    if (toRead < bytes) {
        memset(dst + toRead, 0, bytes - toRead);
    }
    
    return bytes;
}

bool VirtualMicSource::isRendererConnected() const {
    return mRendererConnected.load(std::memory_order_acquire);
}

uint32_t VirtualMicSource::getSampleRate() const {
    std::lock_guard<std::mutex> lock(mLock);
    return mHeader ? mHeader->sampleRate : 48000;
}

uint32_t VirtualMicSource::getChannelCount() const {
    std::lock_guard<std::mutex> lock(mLock);
    return mHeader ? mHeader->channelCount : 1;
}

AudioFormat VirtualMicSource::getFormat() const {
    std::lock_guard<std::mutex> lock(mLock);
    return mHeader ? mHeader->format : AudioFormat::PCM_16_BIT;
}

}  // namespace aidl::android::hardware::audio::virtualmic
