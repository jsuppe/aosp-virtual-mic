/*
 * DevicesFactoryVirtualMic - HIDL @7.1 IDevicesFactory implementation for the
 * virtual microphone HAL. Returns a single DeviceVirtualMic instance.
 */

#pragma once

#include <android/hardware/audio/7.1/IDevicesFactory.h>
#include <hidl/Status.h>

namespace android {
namespace hardware {
namespace audio {
namespace V7_1 {
namespace implementation {
namespace virtualmic {

using ::android::sp;
using ::android::hardware::hidl_string;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::audio::V7_0::Result;

class DevicesFactoryVirtualMic : public V7_1::IDevicesFactory {
public:
    DevicesFactoryVirtualMic();
    virtual ~DevicesFactoryVirtualMic();

    // ::android::hardware::audio::V7_0::IDevicesFactory
    Return<void> openDevice(const hidl_string& device, openDevice_cb _hidl_cb) override;
    Return<void> openPrimaryDevice(openPrimaryDevice_cb _hidl_cb) override;

    // ::android::hardware::audio::V7_1::IDevicesFactory
    Return<void> openDevice_7_1(const hidl_string& device,
                                openDevice_7_1_cb _hidl_cb) override;
    Return<void> openPrimaryDevice_7_1(openPrimaryDevice_7_1_cb _hidl_cb) override;
};

}  // namespace virtualmic
}  // namespace implementation
}  // namespace V7_1
}  // namespace audio
}  // namespace hardware
}  // namespace android
