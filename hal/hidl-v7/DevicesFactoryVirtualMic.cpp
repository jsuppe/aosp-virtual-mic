/*
 * DevicesFactoryVirtualMic - HIDL @7.1 IDevicesFactory implementation.
 */

#define LOG_TAG "VirtualMicDevicesFactory"

#include "DevicesFactoryVirtualMic.h"
#include "DeviceVirtualMic.h"

#include <log/log.h>

namespace android {
namespace hardware {
namespace audio {
namespace V7_1 {
namespace implementation {
namespace virtualmic {

DevicesFactoryVirtualMic::DevicesFactoryVirtualMic() {
    ALOGI("DevicesFactoryVirtualMic created");
}

DevicesFactoryVirtualMic::~DevicesFactoryVirtualMic() {
    ALOGI("DevicesFactoryVirtualMic destroyed");
}

// ::android::hardware::audio::V7_0::IDevicesFactory
Return<void> DevicesFactoryVirtualMic::openDevice(const hidl_string& device,
                                                  openDevice_cb _hidl_cb) {
    ALOGI("openDevice(%s)", device.c_str());
    // We only expose the "virtualmic" module. Anything else is unknown.
    if (device != "virtualmic") {
        ALOGW("Unknown device name requested: %s", device.c_str());
        _hidl_cb(Result::INVALID_ARGUMENTS, nullptr);
        return Void();
    }

    sp<DeviceVirtualMic> dev = new DeviceVirtualMic();
    if (!dev->initialize()) {
        ALOGE("Failed to initialize DeviceVirtualMic");
        _hidl_cb(Result::NOT_INITIALIZED, nullptr);
        return Void();
    }
    _hidl_cb(Result::OK, dev);
    return Void();
}

Return<void> DevicesFactoryVirtualMic::openPrimaryDevice(openPrimaryDevice_cb _hidl_cb) {
    // We are not the primary audio device.
    ALOGW("openPrimaryDevice: not supported by virtualmic");
    _hidl_cb(Result::NOT_SUPPORTED, nullptr);
    return Void();
}

// ::android::hardware::audio::V7_1::IDevicesFactory
Return<void> DevicesFactoryVirtualMic::openDevice_7_1(const hidl_string& device,
                                                      openDevice_7_1_cb _hidl_cb) {
    ALOGI("openDevice_7_1(%s)", device.c_str());
    if (device != "virtualmic") {
        ALOGW("Unknown device name requested: %s", device.c_str());
        _hidl_cb(Result::INVALID_ARGUMENTS, nullptr);
        return Void();
    }

    sp<DeviceVirtualMic> dev = new DeviceVirtualMic();
    if (!dev->initialize()) {
        ALOGE("Failed to initialize DeviceVirtualMic");
        _hidl_cb(Result::NOT_INITIALIZED, nullptr);
        return Void();
    }
    _hidl_cb(Result::OK, dev);
    return Void();
}

Return<void> DevicesFactoryVirtualMic::openPrimaryDevice_7_1(
        openPrimaryDevice_7_1_cb _hidl_cb) {
    ALOGW("openPrimaryDevice_7_1: not supported by virtualmic");
    _hidl_cb(Result::NOT_SUPPORTED, nullptr);
    return Void();
}

}  // namespace virtualmic
}  // namespace implementation
}  // namespace V7_1
}  // namespace audio
}  // namespace hardware
}  // namespace android
