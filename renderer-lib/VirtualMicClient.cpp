/*
 * VirtualMicClient - Client library for apps to inject audio into virtual mic
 */

#include "VirtualMicClient.h"

#include <android/log.h>
#include <android/sharedmem.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

#define LOG_TAG "VirtualMicClient"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace vmic {

// Must match AudioBufferHeader in HAL
struct AudioBufferHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t sampleRate;
    uint32_t channelCount;
    uint32_t format;
    uint32_t bytesPerSample;
    uint32_t ringBufferOffset;
    uint32_t ringBufferSize;
    uint32_t writePos;
    uint32_t readPos;
    uint64_t totalSamplesWritten;
    uint64_t totalSamplesRead;
    uint32_t flags;
    uint32_t reserved[2];
};

constexpr uint32_t AUDIO_BUFFER_MAGIC = 0x43494D56;  // "VMIC"
constexpr uint32_t AUDIO_BUFFER_VERSION = 1;
constexpr uint32_t HEADER_SIZE = 64;

VirtualMicClient::VirtualMicClient() = default;

VirtualMicClient::~VirtualMicClient() {
    shutdown();
}

bool VirtualMicClient::initialize(uint32_t sampleRate, uint32_t channelCount, uint32_t bufferMs) {
    mSampleRate = sampleRate;
    mChannelCount = channelCount;
    mBufferMs = bufferMs;
    
    // Calculate buffer size
    size_t samplesInBuffer = (sampleRate * bufferMs / 1000) * channelCount;
    size_t ringBufferSize = samplesInBuffer * sizeof(int16_t);
    mMappedSize = HEADER_SIZE + ringBufferSize;
    
    LOGI("Creating shared memory: %uHz, %u channels, %ums buffer (%zu bytes)",
         sampleRate, channelCount, bufferMs, mMappedSize);
    
    // Create ashmem
    mShmFd = ASharedMemory_create("virtual_mic_audio", mMappedSize);
    if (mShmFd < 0) {
        LOGE("Failed to create shared memory: %s", strerror(errno));
        return false;
    }
    
    LOGI("Created shared memory: %zu bytes, fd=%d", mMappedSize, mShmFd);
    
    // Map for read/write
    mMappedMemory = mmap(nullptr, mMappedSize, PROT_READ | PROT_WRITE, 
                         MAP_SHARED, mShmFd, 0);
    if (mMappedMemory == MAP_FAILED) {
        LOGE("Failed to mmap: %s", strerror(errno));
        close(mShmFd);
        mShmFd = -1;
        return false;
    }
    
    // Initialize header
    auto* header = static_cast<AudioBufferHeader*>(mMappedMemory);
    memset(header, 0, sizeof(AudioBufferHeader));
    header->magic = AUDIO_BUFFER_MAGIC;
    header->version = AUDIO_BUFFER_VERSION;
    header->sampleRate = sampleRate;
    header->channelCount = channelCount;
    header->format = 1;  // PCM_16_BIT
    header->bytesPerSample = 2;
    header->ringBufferOffset = HEADER_SIZE;
    header->ringBufferSize = ringBufferSize;
    header->writePos = 0;
    header->readPos = 0;
    header->flags = 0x01;  // RENDERER_CONNECTED
    
    // Connect to HAL
    if (!connectToHal()) {
        LOGE("Failed to connect to HAL");
        munmap(mMappedMemory, mMappedSize);
        close(mShmFd);
        mMappedMemory = nullptr;
        mShmFd = -1;
        return false;
    }
    
    mConnected = true;
    LOGI("Initialized: %uHz, %u ch, connected to HAL", sampleRate, channelCount);
    return true;
}

bool VirtualMicClient::connectToHal() {
    int sockFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockFd < 0) {
        LOGE("Failed to create socket: %s", strerror(errno));
        return false;
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    
    // Retry connection
    int maxRetries = 10;
    for (int i = 0; i < maxRetries; i++) {
        if (connect(sockFd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            break;
        }
        if (i == maxRetries - 1) {
            LOGE("Failed to connect after %d attempts: %s", maxRetries, strerror(errno));
            close(sockFd);
            return false;
        }
        LOGI("Waiting for HAL socket (attempt %d/%d)...", i + 1, maxRetries);
        usleep(500000);
    }
    
    LOGI("Connected to HAL socket");
    
    // Send fd via SCM_RIGHTS
    char buf[1] = {0};
    struct iovec iov;
    iov.iov_base = buf;
    iov.iov_len = sizeof(buf);
    
    union {
        struct cmsghdr cmh;
        char control[CMSG_SPACE(sizeof(int))];
    } controlUnion;
    
    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = controlUnion.control;
    msg.msg_controllen = sizeof(controlUnion.control);
    
    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    *reinterpret_cast<int*>(CMSG_DATA(cmsg)) = mShmFd;
    
    if (sendmsg(sockFd, &msg, 0) < 0) {
        LOGE("Failed to send fd: %s", strerror(errno));
        close(sockFd);
        return false;
    }
    
    LOGI("Sent shared memory fd to HAL");
    
    // Send size
    uint64_t size = mMappedSize;
    if (send(sockFd, &size, sizeof(size), 0) != sizeof(size)) {
        LOGE("Failed to send size: %s", strerror(errno));
        close(sockFd);
        return false;
    }
    
    close(sockFd);
    return true;
}

size_t VirtualMicClient::writeSamples(const int16_t* samples, size_t sampleCount) {
    if (!mConnected || mMappedMemory == nullptr) {
        return 0;
    }
    
    auto* header = static_cast<AudioBufferHeader*>(mMappedMemory);
    uint8_t* ringBuffer = static_cast<uint8_t*>(mMappedMemory) + header->ringBufferOffset;
    
    size_t bytesToWrite = sampleCount * mChannelCount * sizeof(int16_t);
    size_t availableBytes = header->ringBufferSize - 
        ((header->writePos - header->readPos + header->ringBufferSize) % header->ringBufferSize) - 1;
    
    bytesToWrite = std::min(bytesToWrite, availableBytes);
    if (bytesToWrite == 0) {
        return 0;  // Buffer full
    }
    
    uint32_t writePos = header->writePos;
    const uint8_t* src = reinterpret_cast<const uint8_t*>(samples);
    
    // Handle wrap-around
    uint32_t toEnd = header->ringBufferSize - writePos;
    if (bytesToWrite <= toEnd) {
        memcpy(ringBuffer + writePos, src, bytesToWrite);
        writePos += bytesToWrite;
        if (writePos >= header->ringBufferSize) {
            writePos = 0;
        }
    } else {
        memcpy(ringBuffer + writePos, src, toEnd);
        memcpy(ringBuffer, src + toEnd, bytesToWrite - toEnd);
        writePos = bytesToWrite - toEnd;
    }
    
    header->writePos = writePos;
    header->totalSamplesWritten += sampleCount;
    
    return sampleCount;
}

size_t VirtualMicClient::availableToWrite() const {
    if (!mConnected || mMappedMemory == nullptr) {
        return 0;
    }
    
    auto* header = static_cast<AudioBufferHeader*>(mMappedMemory);
    size_t used = (header->writePos - header->readPos + header->ringBufferSize) 
                  % header->ringBufferSize;
    return (header->ringBufferSize - used - 1) / (mChannelCount * sizeof(int16_t));
}

void VirtualMicClient::shutdown() {
    if (mMappedMemory != nullptr) {
        munmap(mMappedMemory, mMappedSize);
        mMappedMemory = nullptr;
    }
    if (mShmFd >= 0) {
        close(mShmFd);
        mShmFd = -1;
    }
    mConnected = false;
    LOGI("Shutdown complete");
}

}  // namespace vmic
