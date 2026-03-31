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

#define LOG_TAG "AHAL_ModuleVirtualMic"
#include <android-base/logging.h>

#include "core-impl/ModuleVirtualMic.h"
#include "core-impl/StreamVirtualMic.h"
#include "VirtualMicSource.h"

using aidl::android::hardware::audio::virtualmic::VirtualMicSource;

using aidl::android::hardware::audio::common::SinkMetadata;
using aidl::android::hardware::audio::common::SourceMetadata;
using aidl::android::media::audio::common::AudioOffloadInfo;
using aidl::android::media::audio::common::AudioPortConfig;
using aidl::android::media::audio::common::MicrophoneInfo;

namespace aidl::android::hardware::audio::core {

ModuleVirtualMic::ModuleVirtualMic(std::unique_ptr<Configuration>&& config)
    : Module(Type::VIRTUALMIC, std::move(config)) {
    LOG(INFO) << __func__ << ": Creating VirtualMicSource";
    mSource = std::make_shared<VirtualMicSource>();
    if (!mSource->start()) {
        LOG(ERROR) << __func__ << ": Failed to start VirtualMicSource";
    } else {
        LOG(INFO) << __func__ << ": VirtualMicSource started, waiting for renderer";
    }
}

ModuleVirtualMic::~ModuleVirtualMic() {
    if (mSource) {
        mSource->stop();
    }
}

ndk::ScopedAStatus ModuleVirtualMic::getMicMute(bool* _aidl_return) {
    *_aidl_return = mMicMute;
    LOG(DEBUG) << __func__ << ": returning " << *_aidl_return;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleVirtualMic::setMicMute(bool in_mute) {
    LOG(DEBUG) << __func__ << ": " << in_mute;
    mMicMute = in_mute;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ModuleVirtualMic::createInputStream(
        StreamContext&& context,
        const SinkMetadata& sinkMetadata,
        const std::vector<MicrophoneInfo>& microphones,
        std::shared_ptr<StreamIn>* result) {
    LOG(INFO) << __func__ << ": Creating virtual mic input stream";
    return createStreamInstance<StreamInVirtualMic>(result, std::move(context), 
                                                     sinkMetadata, microphones, mSource);
}

ndk::ScopedAStatus ModuleVirtualMic::createOutputStream(
        StreamContext&& context,
        const SourceMetadata& sourceMetadata,
        const std::optional<AudioOffloadInfo>& offloadInfo,
        std::shared_ptr<StreamOut>* result) {
    // Virtual mic doesn't support output
    (void)context;
    (void)sourceMetadata;
    (void)offloadInfo;
    (void)result;
    LOG(WARNING) << __func__ << ": Virtual mic doesn't support output streams";
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

int32_t ModuleVirtualMic::getNominalLatencyMs(const AudioPortConfig& portConfig) {
    (void)portConfig;
    // Virtual mic latency: buffer + IPC
    return 20;  // 20ms nominal latency
}

}  // namespace aidl::android::hardware::audio::core
