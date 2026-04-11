/*
 * StreamInVirtualMic - HIDL @7.0 IStreamIn implementation for the virtual
 * microphone HAL. Reads PCM from virtualmic::VirtualMicSource (hal/core/)
 * and delivers it via FastMessageQueue following the standard HIDL audio
 * protocol.
 *
 * Note: IDevice 7.1 does NOT override openInputStream, so IStreamIn is the
 * 7.0 type even when served from a 7.1 factory.
 */

#pragma once

#include <android/hardware/audio/7.0/IStreamIn.h>
#include <android/hardware/audio/common/7.0/types.h>
#include <fmq/EventFlag.h>
#include <fmq/MessageQueue.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>
#include <utils/Thread.h>

#include <atomic>
#include <memory>
#include <mutex>

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
using ::android::hardware::kSynchronizedReadWrite;
using ::android::hardware::MessageQueue;
using ::android::hardware::MQDescriptorSync;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::EventFlag;

using ::android::hardware::audio::V7_0::IStreamIn;
using ::android::hardware::audio::V7_0::Result;
using ::android::hardware::audio::V7_0::MicrophoneDirection;
using ::android::hardware::audio::V7_0::MicrophoneInfo;
using ::android::hardware::audio::V7_0::ParameterValue;
using ::android::hardware::audio::common::V7_0::AudioConfig;
using ::android::hardware::audio::common::V7_0::AudioConfigBase;
using ::android::hardware::audio::common::V7_0::AudioConfigBaseOptional;
using ::android::hardware::audio::common::V7_0::AudioProfile;
using ::android::hardware::audio::common::V7_0::DeviceAddress;
using ::android::hardware::audio::common::V7_0::SinkMetadata;

class ReadThread;

class StreamInVirtualMic : public IStreamIn {
public:
    using CommandMQ = MessageQueue<IStreamIn::ReadParameters, kSynchronizedReadWrite>;
    using DataMQ = MessageQueue<uint8_t, kSynchronizedReadWrite>;
    using StatusMQ = MessageQueue<IStreamIn::ReadStatus, kSynchronizedReadWrite>;

    StreamInVirtualMic(std::shared_ptr<::virtualmic::VirtualMicSource> source,
                       const AudioConfig& config);
    virtual ~StreamInVirtualMic();

    // ------- IStream (7.0) -------
    Return<uint64_t> getFrameSize() override;
    Return<uint64_t> getFrameCount() override;
    Return<uint64_t> getBufferSize() override;
    Return<void> getSupportedProfiles(getSupportedProfiles_cb _hidl_cb) override;
    Return<void> getAudioProperties(getAudioProperties_cb _hidl_cb) override;
    Return<Result> setAudioProperties(const AudioConfigBaseOptional& config) override;
    Return<Result> addEffect(uint64_t effectId) override;
    Return<Result> removeEffect(uint64_t effectId) override;
    Return<Result> standby() override;
    Return<void> getDevices(getDevices_cb _hidl_cb) override;
    Return<Result> setDevices(const hidl_vec<DeviceAddress>& devices) override;
    Return<Result> setHwAvSync(uint32_t hwAvSync) override;
    Return<void> getParameters(const hidl_vec<ParameterValue>& context,
                               const hidl_vec<hidl_string>& keys,
                               getParameters_cb _hidl_cb) override;
    Return<Result> setParameters(const hidl_vec<ParameterValue>& context,
                                 const hidl_vec<ParameterValue>& parameters) override;
    Return<Result> start() override;
    Return<Result> stop() override;
    Return<void> createMmapBuffer(int32_t minSizeFrames,
                                  createMmapBuffer_cb _hidl_cb) override;
    Return<void> getMmapPosition(getMmapPosition_cb _hidl_cb) override;
    Return<Result> close() override;

    // ------- IStreamIn (7.0) -------
    Return<void> getAudioSource(getAudioSource_cb _hidl_cb) override;
    Return<Result> setGain(float gain) override;
    Return<Result> updateSinkMetadata(const SinkMetadata& sinkMetadata) override;
    Return<void> prepareForReading(uint32_t frameSize, uint32_t framesCount,
                                   prepareForReading_cb _hidl_cb) override;
    Return<uint32_t> getInputFramesLost() override;
    Return<void> getCapturePosition(getCapturePosition_cb _hidl_cb) override;
    Return<void> getActiveMicrophones(getActiveMicrophones_cb _hidl_cb) override;
    Return<Result> setMicrophoneDirection(MicrophoneDirection direction) override;
    Return<Result> setMicrophoneFieldDimension(float zoom) override;

private:
    std::shared_ptr<::virtualmic::VirtualMicSource> mSource;
    AudioConfig mConfig;
    uint64_t mFrameSizeBytes = 2;   // default mono 16-bit
    uint64_t mFrameCount = 480;     // default 10ms at 48k

    std::atomic<bool> mClosed{false};
    std::atomic<bool> mStandby{true};
    std::atomic<bool> mStarted{false};
    std::atomic<uint64_t> mFramesRead{0};

    // FMQ plumbing
    std::mutex mLock;
    std::unique_ptr<CommandMQ> mCommandMQ;
    std::unique_ptr<DataMQ> mDataMQ;
    std::unique_ptr<StatusMQ> mStatusMQ;
    EventFlag* mEfGroup = nullptr;
    std::atomic<bool> mStopReadThread{false};
    sp<ReadThread> mReadThread;
};

}  // namespace virtualmic
}  // namespace implementation
}  // namespace V7_1
}  // namespace audio
}  // namespace hardware
}  // namespace android
