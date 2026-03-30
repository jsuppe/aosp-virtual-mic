/*
 * Copyright (C) 2024 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <memory>
#include "core-impl/Stream.h"

// Forward declaration with full namespace
namespace aidl::android::hardware::audio::virtualmic {
class VirtualMicSource;
}

namespace aidl::android::hardware::audio::core {

/**
 * StreamVirtualMic - Audio stream that reads from shared memory
 * 
 * Similar to StreamStub but reads actual PCM data from VirtualMicSource
 * instead of generating random data.
 */
class StreamVirtualMic : public StreamCommonImpl {
  public:
    StreamVirtualMic(StreamContext* context, const Metadata& metadata,
                     std::shared_ptr<aidl::android::hardware::audio::virtualmic::VirtualMicSource> source);
    ~StreamVirtualMic();

    // DriverInterface methods
    ::android::status_t init() override;
    ::android::status_t drain(StreamDescriptor::DrainMode) override;
    ::android::status_t flush() override;
    ::android::status_t pause() override;
    ::android::status_t standby() override;
    ::android::status_t start() override;
    ::android::status_t transfer(void* buffer, size_t frameCount, size_t* actualFrameCount,
                                 int32_t* latencyMs) override;
    void shutdown() override;

  private:
    std::shared_ptr<aidl::android::hardware::audio::virtualmic::VirtualMicSource> mSource;
    
    size_t mBufferSizeFrames;
    size_t mFrameSizeBytes;
    uint32_t mSampleRate;
    bool mIsInitialized = false;
    bool mIsStandby = true;
    
    int64_t mStartTimeNs = 0;
    long mFramesSinceStart = 0;
};

class StreamInVirtualMic final : public StreamIn, public StreamVirtualMic {
  public:
    friend class ndk::SharedRefBase;
    StreamInVirtualMic(
            StreamContext&& context,
            const ::aidl::android::hardware::audio::common::SinkMetadata& sinkMetadata,
            const std::vector<::aidl::android::media::audio::common::MicrophoneInfo>& microphones,
            std::shared_ptr<aidl::android::hardware::audio::virtualmic::VirtualMicSource> source);

  private:
    void onClose(StreamDescriptor::State) override { defaultOnClose(); }
};

}  // namespace aidl::android::hardware::audio::core
