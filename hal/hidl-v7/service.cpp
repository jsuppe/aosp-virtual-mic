/*
 * HIDL service entry point for the virtual microphone HAL (@7.1).
 *
 * Registers DevicesFactoryVirtualMic under the "virtualmic" instance name
 * of android.hardware.audio@7.1::IDevicesFactory.
 */

#define LOG_TAG "android.hardware.audio.service.virtualmic"

#include "DevicesFactoryVirtualMic.h"
#include "DeviceVirtualMic.h"

#include <android/hardware/audio/7.1/IDevicesFactory.h>
#include <hidl/HidlTransportSupport.h>
#include <log/log.h>
#include <utils/StrongPointer.h>

using ::android::sp;
using ::android::status_t;
using ::android::hardware::configureRpcThreadpool;
using ::android::hardware::joinRpcThreadpool;
using ::android::hardware::audio::V7_1::IDevicesFactory;
using ::android::hardware::audio::V7_1::implementation::virtualmic::DevicesFactoryVirtualMic;
using ::android::hardware::audio::V7_1::implementation::virtualmic::DeviceVirtualMic;

int main(int /*argc*/, char** /*argv*/) {
    ALOGI("virtualmic audio HAL service starting (HIDL @7.1)");

    configureRpcThreadpool(16, true /*callerWillJoin*/);

    // Eagerly create and initialize a DeviceVirtualMic so the core
    // VirtualMicSource starts its Unix-domain socket server at boot
    // (instead of waiting for a client to call openDevice). This keeps
    // the "renderer connects before audio client" startup order working.
    // The reference is held in a static sp so it stays alive for the
    // lifetime of the process.
    static sp<DeviceVirtualMic> gEagerDevice = new DeviceVirtualMic();
    if (!gEagerDevice->initialize()) {
        ALOGE("Failed to eagerly initialize DeviceVirtualMic — socket will "
              "not be available until a client opens the device");
    } else {
        ALOGI("DeviceVirtualMic eagerly initialized, socket server started");
    }

    sp<IDevicesFactory> factory = new DevicesFactoryVirtualMic();
    status_t status = factory->registerAsService("virtualmic");
    if (status != ::android::OK) {
        ALOGE("failed to register DevicesFactory as 'virtualmic': %d", status);
        return 1;
    }

    ALOGI("virtualmic audio HAL service registered, joining thread pool");
    joinRpcThreadpool();
    return 1;  // joinRpcThreadpool should never return
}
