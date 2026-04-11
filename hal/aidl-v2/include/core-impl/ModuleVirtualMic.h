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
#include "core-impl/Module.h"

// Forward declaration of the version-independent core type.
namespace virtualmic {
class VirtualMicSource;
}

namespace aidl::android::hardware::audio::core {

class ModuleVirtualMic : public Module {
  public:
    ModuleVirtualMic(std::unique_ptr<Configuration>&& config);
    ~ModuleVirtualMic() override;

  private:
    // IModule interfaces
    ndk::ScopedAStatus getMicMute(bool* _aidl_return) override;
    ndk::ScopedAStatus setMicMute(bool in_mute) override;

    // Module interfaces
    ndk::ScopedAStatus createInputStream(
            StreamContext&& context,
            const ::aidl::android::hardware::audio::common::SinkMetadata& sinkMetadata,
            const std::vector<::aidl::android::media::audio::common::MicrophoneInfo>& microphones,
            std::shared_ptr<StreamIn>* result) override;
    ndk::ScopedAStatus createOutputStream(
            StreamContext&& context,
            const ::aidl::android::hardware::audio::common::SourceMetadata& sourceMetadata,
            const std::optional<::aidl::android::media::audio::common::AudioOffloadInfo>&
                    offloadInfo,
            std::shared_ptr<StreamOut>* result) override;
    int32_t getNominalLatencyMs(
            const ::aidl::android::media::audio::common::AudioPortConfig& portConfig) override;

    // Virtual mic source (reads from renderer via shared memory).
    // Lives in the version-independent core namespace.
    std::shared_ptr<::virtualmic::VirtualMicSource> mSource;
    bool mMicMute = false;
};

}  // namespace aidl::android::hardware::audio::core
