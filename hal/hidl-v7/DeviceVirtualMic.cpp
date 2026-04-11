/*
 * DeviceVirtualMic - HIDL @7.1 IDevice implementation.
 */

#define LOG_TAG "VirtualMicDevice"

#include "DeviceVirtualMic.h"
#include "StreamInVirtualMic.h"

// Core pipeline headers (hal/core/)
#include "VirtualMicSource.h"

#include <log/log.h>

namespace android {
namespace hardware {
namespace audio {
namespace V7_1 {
namespace implementation {
namespace virtualmic {

DeviceVirtualMic::DeviceVirtualMic() {
    ALOGI("DeviceVirtualMic created");
}

DeviceVirtualMic::~DeviceVirtualMic() {
    if (mSource) {
        mSource->stop();
        mSource.reset();
    }
    ALOGI("DeviceVirtualMic destroyed");
}

bool DeviceVirtualMic::initialize() {
    if (mInitialized) {
        return true;
    }
    mSource = std::make_shared<::virtualmic::VirtualMicSource>();
    if (!mSource->start()) {
        ALOGE("Failed to start VirtualMicSource");
        mSource.reset();
        return false;
    }
    mInitialized = true;
    ALOGI("DeviceVirtualMic initialized, waiting for renderer...");
    return true;
}

// ------- IDevice (7.0) -------
Return<Result> DeviceVirtualMic::initCheck() {
    return mInitialized ? Result::OK : Result::NOT_INITIALIZED;
}

Return<Result> DeviceVirtualMic::setMasterVolume(float /*volume*/) {
    return Result::NOT_SUPPORTED;
}

Return<void> DeviceVirtualMic::getMasterVolume(getMasterVolume_cb _hidl_cb) {
    _hidl_cb(Result::NOT_SUPPORTED, 1.0f);
    return Void();
}

Return<Result> DeviceVirtualMic::setMicMute(bool /*mute*/) {
    return Result::NOT_SUPPORTED;
}

Return<void> DeviceVirtualMic::getMicMute(getMicMute_cb _hidl_cb) {
    _hidl_cb(Result::NOT_SUPPORTED, false);
    return Void();
}

Return<Result> DeviceVirtualMic::setMasterMute(bool /*mute*/) {
    return Result::NOT_SUPPORTED;
}

Return<void> DeviceVirtualMic::getMasterMute(getMasterMute_cb _hidl_cb) {
    _hidl_cb(Result::NOT_SUPPORTED, false);
    return Void();
}

Return<void> DeviceVirtualMic::getInputBufferSize(const AudioConfig& /*config*/,
                                                  getInputBufferSize_cb _hidl_cb) {
    // 10ms worth of 16-bit mono at 48kHz => 480 frames * 2 bytes = 960 bytes.
    // Return a reasonable default regardless of the requested config — the
    // stream will negotiate via suggestedConfig.
    constexpr uint64_t kBufferBytes = 480 * 2;
    _hidl_cb(Result::OK, kBufferBytes);
    return Void();
}

Return<void> DeviceVirtualMic::openOutputStream(int32_t /*ioHandle*/,
                                                const DeviceAddress& /*device*/,
                                                const AudioConfig& config,
                                                const hidl_vec<hidl_string>& /*flags*/,
                                                const SourceMetadata& /*sourceMetadata*/,
                                                openOutputStream_cb _hidl_cb) {
    // Virtual microphone is input-only.
    _hidl_cb(Result::NOT_SUPPORTED, nullptr, config);
    return Void();
}

Return<void> DeviceVirtualMic::openInputStream(int32_t /*ioHandle*/,
                                               const DeviceAddress& /*device*/,
                                               const AudioConfig& config,
                                               const hidl_vec<hidl_string>& /*flags*/,
                                               const SinkMetadata& /*sinkMetadata*/,
                                               openInputStream_cb _hidl_cb) {
    ALOGI("openInputStream: sampleRateHz=%u, format='%s', channelMask='%s'",
          config.base.sampleRateHz,
          config.base.format.c_str(),
          config.base.channelMask.c_str());

    if (!mInitialized || !mSource) {
        ALOGE("openInputStream called before initialize()");
        _hidl_cb(Result::NOT_INITIALIZED, nullptr, config);
        return Void();
    }

    sp<StreamInVirtualMic> stream = new StreamInVirtualMic(mSource, config);
    _hidl_cb(Result::OK, stream, config);
    return Void();
}

Return<bool> DeviceVirtualMic::supportsAudioPatches() {
    return false;
}

Return<void> DeviceVirtualMic::createAudioPatch(const hidl_vec<AudioPortConfig>& /*sources*/,
                                                const hidl_vec<AudioPortConfig>& /*sinks*/,
                                                createAudioPatch_cb _hidl_cb) {
    _hidl_cb(Result::NOT_SUPPORTED, AudioPatchHandle{});
    return Void();
}

Return<void> DeviceVirtualMic::updateAudioPatch(int32_t /*previousPatch*/,
                                                const hidl_vec<AudioPortConfig>& /*sources*/,
                                                const hidl_vec<AudioPortConfig>& /*sinks*/,
                                                updateAudioPatch_cb _hidl_cb) {
    _hidl_cb(Result::NOT_SUPPORTED, AudioPatchHandle{});
    return Void();
}

Return<Result> DeviceVirtualMic::releaseAudioPatch(int32_t /*patch*/) {
    return Result::NOT_SUPPORTED;
}

Return<void> DeviceVirtualMic::getAudioPort(const AudioPort& port, getAudioPort_cb _hidl_cb) {
    _hidl_cb(Result::NOT_SUPPORTED, port);
    return Void();
}

Return<Result> DeviceVirtualMic::setAudioPortConfig(const AudioPortConfig& /*config*/) {
    return Result::NOT_SUPPORTED;
}

Return<void> DeviceVirtualMic::getHwAvSync(getHwAvSync_cb _hidl_cb) {
    _hidl_cb(Result::NOT_SUPPORTED, AudioHwSync{});
    return Void();
}

Return<Result> DeviceVirtualMic::setScreenState(bool /*turnedOn*/) {
    return Result::OK;
}

Return<void> DeviceVirtualMic::getParameters(const hidl_vec<ParameterValue>& /*context*/,
                                             const hidl_vec<hidl_string>& keys,
                                             getParameters_cb _hidl_cb) {
    if (keys.size() == 0) {
        _hidl_cb(Result::OK, hidl_vec<ParameterValue>{});
    } else {
        _hidl_cb(Result::NOT_SUPPORTED, hidl_vec<ParameterValue>{});
    }
    return Void();
}

Return<Result> DeviceVirtualMic::setParameters(const hidl_vec<ParameterValue>& /*context*/,
                                               const hidl_vec<ParameterValue>& /*parameters*/) {
    // Accept anything — we don't interpret parameters.
    return Result::OK;
}

Return<void> DeviceVirtualMic::getMicrophones(getMicrophones_cb _hidl_cb) {
    _hidl_cb(Result::NOT_SUPPORTED, hidl_vec<MicrophoneInfo>{});
    return Void();
}

Return<Result> DeviceVirtualMic::setConnectedState(const DeviceAddress& /*address*/,
                                                   bool /*connected*/) {
    return Result::OK;
}

Return<Result> DeviceVirtualMic::close() {
    if (mClosed) {
        return Result::INVALID_STATE;
    }
    if (mSource) {
        mSource->stop();
        mSource.reset();
    }
    mClosed = true;
    mInitialized = false;
    return Result::OK;
}

Return<Result> DeviceVirtualMic::addDeviceEffect(AudioPortHandle /*device*/,
                                                 uint64_t /*effectId*/) {
    return Result::NOT_SUPPORTED;
}

Return<Result> DeviceVirtualMic::removeDeviceEffect(AudioPortHandle /*device*/,
                                                    uint64_t /*effectId*/) {
    return Result::NOT_SUPPORTED;
}

// ------- IDevice (7.1) -------
Return<void> DeviceVirtualMic::openOutputStream_7_1(int32_t /*ioHandle*/,
                                                    const DeviceAddress& /*device*/,
                                                    const AudioConfig& config,
                                                    const hidl_vec<hidl_string>& /*flags*/,
                                                    const SourceMetadata& /*sourceMetadata*/,
                                                    openOutputStream_7_1_cb _hidl_cb) {
    _hidl_cb(Result::NOT_SUPPORTED, nullptr, config);
    return Void();
}

Return<Result> DeviceVirtualMic::setConnectedState_7_1(const AudioPort& /*devicePort*/,
                                                       bool /*connected*/) {
    return Result::OK;
}

}  // namespace virtualmic
}  // namespace implementation
}  // namespace V7_1
}  // namespace audio
}  // namespace hardware
}  // namespace android
