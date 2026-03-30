/*
 * VirtualMicModule - Simplified audio module for virtual microphone
 */

#define LOG_TAG "VirtualMicModule"

#include "VirtualMicModule.h"

#include <log/log.h>

namespace aidl::android::hardware::audio::virtualmic {

VirtualMicModule::VirtualMicModule() {
    ALOGI("VirtualMicModule created");
}

VirtualMicModule::~VirtualMicModule() {
    shutdown();
    ALOGI("VirtualMicModule destroyed");
}

bool VirtualMicModule::initialize() {
    std::lock_guard<std::mutex> lock(mLock);
    
    if (mInitialized) {
        return true;
    }
    
    // Create the audio source
    mSource = std::make_shared<VirtualMicSource>();
    if (!mSource->start()) {
        ALOGE("Failed to start VirtualMicSource");
        mSource.reset();
        return false;
    }
    
    mInitialized = true;
    ALOGI("VirtualMicModule initialized, waiting for renderer...");
    return true;
}

void VirtualMicModule::shutdown() {
    std::lock_guard<std::mutex> lock(mLock);
    
    if (mActiveStream) {
        mActiveStream->stop();
        mActiveStream.reset();
    }
    
    if (mSource) {
        mSource->stop();
        mSource.reset();
    }
    
    mInitialized = false;
    ALOGI("VirtualMicModule shutdown");
}

std::shared_ptr<VirtualMicStream> VirtualMicModule::openInputStream(const StreamConfig& config) {
    std::lock_guard<std::mutex> lock(mLock);
    
    if (!mInitialized) {
        ALOGE("Module not initialized");
        return nullptr;
    }
    
    if (mActiveStream) {
        ALOGW("Input stream already open, closing previous");
        mActiveStream->stop();
        mActiveStream.reset();
    }
    
    mActiveStream = std::make_shared<VirtualMicStream>(mSource, config);
    
    ALOGI("Opened input stream: %uHz, %u channels",
          config.sampleRate, config.channelCount);
    
    return mActiveStream;
}

void VirtualMicModule::closeInputStream(std::shared_ptr<VirtualMicStream> stream) {
    std::lock_guard<std::mutex> lock(mLock);
    
    if (stream && stream == mActiveStream) {
        mActiveStream->stop();
        mActiveStream.reset();
        ALOGI("Closed input stream");
    }
}

bool VirtualMicModule::isRendererConnected() const {
    std::lock_guard<std::mutex> lock(mLock);
    return mSource && mSource->isRendererConnected();
}

}  // namespace aidl::android::hardware::audio::virtualmic
