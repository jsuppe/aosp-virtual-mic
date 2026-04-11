/*
 * HIDL service entry point for the virtual microphone HAL (@7.1).
 *
 * Registers DevicesFactoryVirtualMic under the "virtualmic" instance name
 * of android.hardware.audio@7.1::IDevicesFactory.
 */

#define LOG_TAG "android.hardware.audio.service.virtualmic"

#include "DevicesFactoryVirtualMic.h"

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

int main(int /*argc*/, char** /*argv*/) {
    ALOGI("virtualmic audio HAL service starting (HIDL @7.1)");

    configureRpcThreadpool(16, true /*callerWillJoin*/);

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
