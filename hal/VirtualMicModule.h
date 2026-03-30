/*
 * VirtualMicModule - Simplified audio module for virtual microphone
 *
 * This provides a minimal audio input device that reads from shared memory.
 * For full AOSP integration, this would need to extend the base Module class.
 */

#pragma once

#include <memory>
#include <mutex>
#include <string>

#include "VirtualMicSource.h"
#include "VirtualMicStream.h"

namespace aidl::android::hardware::audio::virtualmic {

/**
 * Microphone device info
 */
struct MicrophoneInfo {
    std::string id = "virtual_mic_0";
    std::string description = "Virtual Microphone";
    uint32_t deviceId = 100;  // Unique device ID
};

/**
 * VirtualMicModule - Provides virtual microphone capability
 *
 * This is a simplified module that:
 * 1. Registers as an audio input device
 * 2. Creates VirtualMicStream when input is requested
 * 3. Manages VirtualMicSource for shared memory IPC
 */
class VirtualMicModule {
public:
    VirtualMicModule();
    ~VirtualMicModule();
    
    // Initialize the module (start socket server)
    bool initialize();
    
    // Shutdown
    void shutdown();
    
    // Open an input stream
    std::shared_ptr<VirtualMicStream> openInputStream(const StreamConfig& config);
    
    // Close input stream
    void closeInputStream(std::shared_ptr<VirtualMicStream> stream);
    
    // Get microphone info
    MicrophoneInfo getMicrophoneInfo() const { return mMicInfo; }
    
    // Check if renderer is connected
    bool isRendererConnected() const;
    
private:
    std::shared_ptr<VirtualMicSource> mSource;
    std::shared_ptr<VirtualMicStream> mActiveStream;
    MicrophoneInfo mMicInfo;
    
    mutable std::mutex mLock;
    bool mInitialized = false;
};

}  // namespace aidl::android::hardware::audio::virtualmic
