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

/*
 * Standalone service entry point for the Virtual Microphone Audio HAL
 * (AIDL V2, Android 14+).
 *
 * NOTE: In the primary deployment path, ModuleVirtualMic is loaded by the
 * existing `android.hardware.audio.service` binary in AOSP (see
 * hardware/interfaces/audio/aidl/default/Module.cpp, which dispatches on
 * Type::VIRTUALMIC). This service.cpp is provided so that the HAL can
 * also be built and run as its own standalone binary for development and
 * isolation testing.
 */

#define LOG_TAG "VirtualMicService"

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

#include "core-impl/ModuleVirtualMic.h"
#include "core-impl/Configuration.h"

using aidl::android::hardware::audio::core::Configuration;
using aidl::android::hardware::audio::core::Module;
using aidl::android::hardware::audio::core::ModuleVirtualMic;

int main() {
    // Single binder thread is sufficient for the mic module; the audio
    // source runs its own dedicated accept thread.
    ABinderProcess_setThreadPoolMaxThreadCount(16);
    ABinderProcess_startThreadPool();

    // Build a default Configuration for the virtualmic module. In the
    // integrated path, AOSP's Configuration.cpp provides this via
    // getRSubmixConfiguration(); here we fall back to a fresh instance.
    auto config = std::make_unique<Configuration>();
    auto module = ndk::SharedRefBase::make<ModuleVirtualMic>(std::move(config));

    const std::string instance =
            std::string(Module::descriptor) + "/virtualmic";
    binder_status_t status =
            AServiceManager_addService(module->asBinder().get(), instance.c_str());
    if (status != STATUS_OK) {
        LOG(FATAL) << "Failed to register " << instance << " (status=" << status << ")";
    }
    LOG(INFO) << "Registered " << instance;

    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;  // joinThreadPool should not return
}
