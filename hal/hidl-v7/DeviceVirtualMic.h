/*
 * DeviceVirtualMic - HIDL @7.1 IDevice implementation for the virtual
 * microphone HAL. Owns a virtualmic::VirtualMicSource (from hal/core/) and
 * hands it out to StreamInVirtualMic on openInputStream.
 */

#pragma once

#include <android/hardware/audio/7.1/IDevice.h>
#include <hidl/Status.h>

#include <memory>

// Forward-declared core type lives in hal/core/ (plain C++ namespace).
namespace virtualmic {
class VirtualMicSource;
}  // namespace virtualmic

namespace android {
namespace hardware {
namespace audio {
namespace V7_1 {
namespace implementation {
namespace virtualmic {

using ::android::sp;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::hidl_handle;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::audio::V7_0::Result;
using ::android::hardware::audio::V7_0::IStreamIn;
using ::android::hardware::audio::common::V7_0::AudioConfig;
using ::android::hardware::audio::common::V7_0::AudioPatchHandle;
using ::android::hardware::audio::common::V7_0::AudioPort;
using ::android::hardware::audio::common::V7_0::AudioPortConfig;
using ::android::hardware::audio::common::V7_0::AudioPortHandle;
using ::android::hardware::audio::common::V7_0::DeviceAddress;
using ::android::hardware::audio::common::V7_0::SinkMetadata;
using ::android::hardware::audio::common::V7_0::SourceMetadata;
using ::android::hardware::audio::common::V7_0::AudioIoHandle;
using ::android::hardware::audio::common::V7_0::AudioHwSync;
using ::android::hardware::audio::V7_0::ParameterValue;
using ::android::hardware::audio::V7_0::MicrophoneInfo;

class DeviceVirtualMic : public V7_1::IDevice {
public:
    DeviceVirtualMic();
    virtual ~DeviceVirtualMic();

    // Starts the core source (socket server). Must be called before any
    // openInputStream. Returns false if the core source failed to start.
    bool initialize();

    // ------- IDevice (7.0) -------
    Return<Result> initCheck() override;
    Return<Result> setMasterVolume(float volume) override;
    Return<void> getMasterVolume(getMasterVolume_cb _hidl_cb) override;
    Return<Result> setMicMute(bool mute) override;
    Return<void> getMicMute(getMicMute_cb _hidl_cb) override;
    Return<Result> setMasterMute(bool mute) override;
    Return<void> getMasterMute(getMasterMute_cb _hidl_cb) override;
    Return<void> getInputBufferSize(const AudioConfig& config,
                                    getInputBufferSize_cb _hidl_cb) override;

    Return<void> openOutputStream(int32_t ioHandle, const DeviceAddress& device,
                                  const AudioConfig& config,
                                  const hidl_vec<hidl_string>& flags,
                                  const SourceMetadata& sourceMetadata,
                                  openOutputStream_cb _hidl_cb) override;

    Return<void> openInputStream(int32_t ioHandle, const DeviceAddress& device,
                                 const AudioConfig& config,
                                 const hidl_vec<hidl_string>& flags,
                                 const SinkMetadata& sinkMetadata,
                                 openInputStream_cb _hidl_cb) override;

    Return<bool> supportsAudioPatches() override;
    Return<void> createAudioPatch(const hidl_vec<AudioPortConfig>& sources,
                                  const hidl_vec<AudioPortConfig>& sinks,
                                  createAudioPatch_cb _hidl_cb) override;
    Return<void> updateAudioPatch(int32_t previousPatch,
                                  const hidl_vec<AudioPortConfig>& sources,
                                  const hidl_vec<AudioPortConfig>& sinks,
                                  updateAudioPatch_cb _hidl_cb) override;
    Return<Result> releaseAudioPatch(int32_t patch) override;
    Return<void> getAudioPort(const AudioPort& port, getAudioPort_cb _hidl_cb) override;
    Return<Result> setAudioPortConfig(const AudioPortConfig& config) override;
    Return<void> getHwAvSync(getHwAvSync_cb _hidl_cb) override;
    Return<Result> setScreenState(bool turnedOn) override;
    Return<void> getParameters(const hidl_vec<ParameterValue>& context,
                               const hidl_vec<hidl_string>& keys,
                               getParameters_cb _hidl_cb) override;
    Return<Result> setParameters(const hidl_vec<ParameterValue>& context,
                                 const hidl_vec<ParameterValue>& parameters) override;
    Return<void> getMicrophones(getMicrophones_cb _hidl_cb) override;
    Return<Result> setConnectedState(const DeviceAddress& address, bool connected) override;
    Return<Result> close() override;
    Return<Result> addDeviceEffect(AudioPortHandle device, uint64_t effectId) override;
    Return<Result> removeDeviceEffect(AudioPortHandle device, uint64_t effectId) override;

    // ------- IDevice (7.1) -------
    Return<void> openOutputStream_7_1(int32_t ioHandle, const DeviceAddress& device,
                                      const AudioConfig& config,
                                      const hidl_vec<hidl_string>& flags,
                                      const SourceMetadata& sourceMetadata,
                                      openOutputStream_7_1_cb _hidl_cb) override;
    Return<Result> setConnectedState_7_1(const AudioPort& devicePort, bool connected) override;

private:
    std::shared_ptr<::virtualmic::VirtualMicSource> mSource;
    bool mInitialized = false;
    bool mClosed = false;
};

}  // namespace virtualmic
}  // namespace implementation
}  // namespace V7_1
}  // namespace audio
}  // namespace hardware
}  // namespace android
