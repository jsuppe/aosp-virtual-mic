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

#define LOG_TAG "AHAL_StreamVirtualMic"
#include <android-base/logging.h>
#include <audio_utils/clock.h>
#include <cstring>

#include "core-impl/StreamVirtualMic.h"

// Core pipeline (version-independent).
#include "VirtualMicSource.h"

using ::virtualmic::VirtualMicSource;

using aidl::android::hardware::audio::common::SinkMetadata;
using aidl::android::media::audio::common::MicrophoneInfo;

namespace aidl::android::hardware::audio::core {

StreamVirtualMic::StreamVirtualMic(StreamContext* context, const Metadata& metadata,
                                   std::shared_ptr<VirtualMicSource> source)
    : StreamCommonImpl(context, metadata),
      mSource(source),
      mBufferSizeFrames(context->getBufferSizeInFrames()),
      mFrameSizeBytes(context->getFrameSize()),
      mSampleRate(context->getSampleRate()) {
    LOG(DEBUG) << __func__ << ": buffer=" << mBufferSizeFrames
               << " frames, frameSize=" << mFrameSizeBytes
               << ", sampleRate=" << mSampleRate;
}

StreamVirtualMic::~StreamVirtualMic() {
    LOG(DEBUG) << __func__;
}

::android::status_t StreamVirtualMic::init() {
    mIsInitialized = true;
    LOG(DEBUG) << __func__ << ": initialized";
    return ::android::OK;
}

::android::status_t StreamVirtualMic::drain(StreamDescriptor::DrainMode) {
    if (!mIsInitialized) {
        LOG(FATAL) << __func__ << ": not initialized";
    }
    return ::android::OK;
}

::android::status_t StreamVirtualMic::flush() {
    if (!mIsInitialized) {
        LOG(FATAL) << __func__ << ": not initialized";
    }
    return ::android::OK;
}

::android::status_t StreamVirtualMic::pause() {
    if (!mIsInitialized) {
        LOG(FATAL) << __func__ << ": not initialized";
    }
    return ::android::OK;
}

::android::status_t StreamVirtualMic::standby() {
    if (!mIsInitialized) {
        LOG(FATAL) << __func__ << ": not initialized";
    }
    mIsStandby = true;
    LOG(VERBOSE) << __func__;
    return ::android::OK;
}

::android::status_t StreamVirtualMic::start() {
    if (!mIsInitialized) {
        LOG(FATAL) << __func__ << ": not initialized";
    }
    mIsStandby = false;
    mStartTimeNs = ::android::uptimeNanos();
    mFramesSinceStart = 0;
    LOG(VERBOSE) << __func__;
    return ::android::OK;
}

::android::status_t StreamVirtualMic::transfer(void* buffer, size_t frameCount,
                                                size_t* actualFrameCount, int32_t* latencyMs) {
    if (!mIsInitialized) {
        LOG(FATAL) << __func__ << ": not initialized";
    }
    if (mIsStandby) {
        LOG(FATAL) << __func__ << ": called while in standby";
    }

    // Read audio data from shared memory source
    size_t bytesToRead = frameCount * mFrameSizeBytes;
    size_t bytesRead = mSource->read(buffer, bytesToRead);
    *actualFrameCount = bytesRead / mFrameSizeBytes;

    // Calculate latency
    if (latencyMs) {
        *latencyMs = 20;  // Nominal 20ms latency
    }

    // Timing: simulate real-time audio capture rate
    mFramesSinceStart += *actualFrameCount;
    static constexpr float kMicrosPerSecond = MICROS_PER_SECOND;
    const long bufferDurationUs = (*actualFrameCount) * kMicrosPerSecond / mSampleRate;
    const auto totalDurationUs =
            (::android::uptimeNanos() - mStartTimeNs) / NANOS_PER_MICROSECOND;
    const long totalOffsetUs =
            mFramesSinceStart * kMicrosPerSecond / mSampleRate - totalDurationUs;

    if (totalOffsetUs > 0) {
        const long sleepTimeUs = std::min(totalOffsetUs, bufferDurationUs);
        LOG(VERBOSE) << __func__ << ": sleeping for " << sleepTimeUs << " us";
        usleep(sleepTimeUs);
    }

    return ::android::OK;
}

void StreamVirtualMic::shutdown() {
    mIsInitialized = false;
    LOG(DEBUG) << __func__;
}

// StreamInVirtualMic

StreamInVirtualMic::StreamInVirtualMic(
        StreamContext&& context,
        const SinkMetadata& sinkMetadata,
        const std::vector<MicrophoneInfo>& microphones,
        std::shared_ptr<VirtualMicSource> source)
    : StreamIn(std::move(context), microphones),
      StreamVirtualMic(&mContextInstance, sinkMetadata, source) {
    LOG(DEBUG) << __func__;
}

}  // namespace aidl::android::hardware::audio::core
