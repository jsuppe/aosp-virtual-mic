/*
 * StreamInVirtualMic - HIDL @7.0 IStreamIn implementation.
 *
 * The FMQ protocol below mirrors the reference default audio HAL
 * (hardware/interfaces/audio/core/all-versions/default/StreamIn.cpp):
 *
 *   1. prepareForReading() creates three SynchronizedReadWrite MQs:
 *        - commandMQ : IStreamIn::ReadParameters (READ / GET_CAPTURE_POSITION)
 *        - dataMQ    : raw PCM bytes (frameSize * framesCount)
 *        - statusMQ  : IStreamIn::ReadStatus
 *      plus an EventFlag created from the dataMQ's event flag word.
 *   2. The HAL-side ReadThread waits on NOT_FULL, pops a command, performs
 *      the read from VirtualMicSource, writes bytes into dataMQ, writes
 *      status into statusMQ, then wakes NOT_EMPTY so the client side
 *      (AudioFlinger) can consume.
 */

#define LOG_TAG "VirtualMicStreamIn"

#include "StreamInVirtualMic.h"

// Core pipeline header (hal/core/)
#include "VirtualMicSource.h"

#include <log/log.h>
#include <utils/Thread.h>

#include <cstring>
#include <memory>
#include <new>

namespace android {
namespace hardware {
namespace audio {
namespace V7_1 {
namespace implementation {
namespace virtualmic {

using ::android::hardware::audio::V7_0::MessageQueueFlagBits;

// Max FMQ buffer we will accept.
static constexpr uint32_t kMaxBufferBytes = 1 << 20;  // 1 MiB

// ---------------------------------------------------------------------------
// ReadThread — polls commandMQ, reads from source, writes dataMQ/statusMQ.
// ---------------------------------------------------------------------------
class ReadThread : public ::android::Thread {
public:
    ReadThread(std::atomic<bool>* stop,
               std::shared_ptr<::virtualmic::VirtualMicSource> source,
               StreamInVirtualMic::CommandMQ* commandMQ,
               StreamInVirtualMic::DataMQ* dataMQ,
               StreamInVirtualMic::StatusMQ* statusMQ,
               EventFlag* efGroup,
               std::atomic<uint64_t>* framesRead,
               uint32_t /*sampleRateHz*/,
               uint64_t frameSizeBytes)
        : Thread(false /*canCallJava*/),
          mStop(stop),
          mSource(std::move(source)),
          mCommandMQ(commandMQ),
          mDataMQ(dataMQ),
          mStatusMQ(statusMQ),
          mEfGroup(efGroup),
          mFramesRead(framesRead),
          mFrameSizeBytes(frameSizeBytes) {}

    bool init() {
        mBuffer.reset(new (std::nothrow) uint8_t[mDataMQ->getQuantumCount()]);
        return mBuffer != nullptr;
    }

    ~ReadThread() override = default;

private:
    std::atomic<bool>* mStop;
    std::shared_ptr<::virtualmic::VirtualMicSource> mSource;
    StreamInVirtualMic::CommandMQ* mCommandMQ;
    StreamInVirtualMic::DataMQ* mDataMQ;
    StreamInVirtualMic::StatusMQ* mStatusMQ;
    EventFlag* mEfGroup;
    std::atomic<uint64_t>* mFramesRead;
    uint64_t mFrameSizeBytes;

    std::unique_ptr<uint8_t[]> mBuffer;
    IStreamIn::ReadParameters mParameters{};
    IStreamIn::ReadStatus mStatus{};

    bool threadLoop() override;
    void doRead();
    void doGetCapturePosition();
};

void ReadThread::doRead() {
    size_t availableToWrite = mDataMQ->availableToWrite();
    size_t requestedToRead = mParameters.params.read;
    if (requestedToRead > availableToWrite) {
        ALOGW("truncating read from %zu to %zu due to insufficient queue space",
              requestedToRead, availableToWrite);
        requestedToRead = availableToWrite;
    }

    size_t bytesRead = 0;
    if (mSource && requestedToRead > 0) {
        bytesRead = mSource->read(mBuffer.get(), requestedToRead);
    } else if (requestedToRead > 0) {
        memset(mBuffer.get(), 0, requestedToRead);
        bytesRead = requestedToRead;
    }

    mStatus.retval = Result::OK;
    mStatus.reply.read = bytesRead;

    if (bytesRead > 0) {
        if (!mDataMQ->write(mBuffer.get(), bytesRead)) {
            ALOGW("data message queue write failed");
        }
        if (mFrameSizeBytes > 0) {
            mFramesRead->fetch_add(bytesRead / mFrameSizeBytes, std::memory_order_relaxed);
        }
    }
}

void ReadThread::doGetCapturePosition() {
    mStatus.retval = Result::OK;
    mStatus.reply.capturePosition.frames =
            mFramesRead->load(std::memory_order_relaxed);
    // Monotonic time in nanoseconds; a coarse but valid value.
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    mStatus.reply.capturePosition.time =
            static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL +
            static_cast<uint64_t>(ts.tv_nsec);
}

bool ReadThread::threadLoop() {
    // Drive-loop: never return control to base Thread until stop signalled,
    // to avoid priority inversion on internal mutexes.
    while (!mStop->load(std::memory_order_acquire)) {
        uint32_t efState = 0;
        mEfGroup->wait(static_cast<uint32_t>(MessageQueueFlagBits::NOT_FULL), &efState);
        if (!(efState & static_cast<uint32_t>(MessageQueueFlagBits::NOT_FULL))) {
            continue;
        }
        if (!mCommandMQ->read(&mParameters)) {
            continue;
        }
        mStatus.replyTo = mParameters.command;
        switch (mParameters.command) {
            case IStreamIn::ReadCommand::READ:
                doRead();
                break;
            case IStreamIn::ReadCommand::GET_CAPTURE_POSITION:
                doGetCapturePosition();
                break;
            default:
                ALOGE("Unknown read thread command %d",
                      static_cast<int>(mParameters.command));
                mStatus.retval = Result::NOT_SUPPORTED;
                break;
        }
        if (!mStatusMQ->write(&mStatus)) {
            ALOGW("status message queue write failed");
        }
        mEfGroup->wake(static_cast<uint32_t>(MessageQueueFlagBits::NOT_EMPTY));
    }
    return false;
}

// ---------------------------------------------------------------------------
// StreamInVirtualMic
// ---------------------------------------------------------------------------

StreamInVirtualMic::StreamInVirtualMic(
        std::shared_ptr<::virtualmic::VirtualMicSource> source,
        const AudioConfig& config)
    : mSource(std::move(source)), mConfig(config) {
    ALOGI("StreamInVirtualMic created: sampleRateHz=%u format='%s' channelMask='%s'",
          mConfig.base.sampleRateHz, mConfig.base.format.c_str(),
          mConfig.base.channelMask.c_str());
}

StreamInVirtualMic::~StreamInVirtualMic() {
    close();
    if (mReadThread.get() != nullptr) {
        status_t status = mReadThread->join();
        ALOGE_IF(status, "read thread exit error: %s", strerror(-status));
        mReadThread.clear();
    }
    if (mEfGroup) {
        status_t status = EventFlag::deleteEventFlag(&mEfGroup);
        ALOGE_IF(status, "read MQ event flag deletion error: %s", strerror(-status));
        mEfGroup = nullptr;
    }
    ALOGI("StreamInVirtualMic destroyed");
}

// ------- IStream -------
Return<uint64_t> StreamInVirtualMic::getFrameSize() {
    return mFrameSizeBytes;
}

Return<uint64_t> StreamInVirtualMic::getFrameCount() {
    return mFrameCount;
}

Return<uint64_t> StreamInVirtualMic::getBufferSize() {
    return mFrameCount * mFrameSizeBytes;
}

Return<void> StreamInVirtualMic::getSupportedProfiles(getSupportedProfiles_cb _hidl_cb) {
    _hidl_cb(Result::NOT_SUPPORTED, hidl_vec<AudioProfile>{});
    return Void();
}

Return<void> StreamInVirtualMic::getAudioProperties(getAudioProperties_cb _hidl_cb) {
    _hidl_cb(Result::OK, mConfig.base);
    return Void();
}

Return<Result> StreamInVirtualMic::setAudioProperties(const AudioConfigBaseOptional& /*c*/) {
    return Result::NOT_SUPPORTED;
}

Return<Result> StreamInVirtualMic::addEffect(uint64_t /*effectId*/) {
    return Result::NOT_SUPPORTED;
}

Return<Result> StreamInVirtualMic::removeEffect(uint64_t /*effectId*/) {
    return Result::NOT_SUPPORTED;
}

Return<Result> StreamInVirtualMic::standby() {
    mStandby.store(true, std::memory_order_release);
    return Result::OK;
}

Return<void> StreamInVirtualMic::getDevices(getDevices_cb _hidl_cb) {
    _hidl_cb(Result::OK, hidl_vec<DeviceAddress>{});
    return Void();
}

Return<Result> StreamInVirtualMic::setDevices(const hidl_vec<DeviceAddress>& /*devices*/) {
    return Result::OK;
}

Return<Result> StreamInVirtualMic::setHwAvSync(uint32_t /*hwAvSync*/) {
    return Result::NOT_SUPPORTED;
}

Return<void> StreamInVirtualMic::getParameters(const hidl_vec<ParameterValue>& /*ctx*/,
                                               const hidl_vec<hidl_string>& keys,
                                               getParameters_cb _hidl_cb) {
    if (keys.size() == 0) {
        _hidl_cb(Result::OK, hidl_vec<ParameterValue>{});
    } else {
        _hidl_cb(Result::NOT_SUPPORTED, hidl_vec<ParameterValue>{});
    }
    return Void();
}

Return<Result> StreamInVirtualMic::setParameters(const hidl_vec<ParameterValue>& /*ctx*/,
                                                 const hidl_vec<ParameterValue>& /*p*/) {
    return Result::OK;
}

Return<Result> StreamInVirtualMic::start() {
    // Non-mmap stream; start/stop are no-ops for PCM streams.
    mStarted.store(true, std::memory_order_release);
    mStandby.store(false, std::memory_order_release);
    return Result::OK;
}

Return<Result> StreamInVirtualMic::stop() {
    mStarted.store(false, std::memory_order_release);
    return Result::OK;
}

Return<void> StreamInVirtualMic::createMmapBuffer(int32_t /*minSizeFrames*/,
                                                  createMmapBuffer_cb _hidl_cb) {
    _hidl_cb(Result::NOT_SUPPORTED,
             ::android::hardware::audio::V7_0::MmapBufferInfo{});
    return Void();
}

Return<void> StreamInVirtualMic::getMmapPosition(getMmapPosition_cb _hidl_cb) {
    _hidl_cb(Result::NOT_SUPPORTED,
             ::android::hardware::audio::V7_0::MmapPosition{});
    return Void();
}

Return<Result> StreamInVirtualMic::close() {
    if (mClosed.exchange(true, std::memory_order_acq_rel)) {
        return Result::INVALID_STATE;
    }
    mStopReadThread.store(true, std::memory_order_release);
    if (mEfGroup) {
        mEfGroup->wake(static_cast<uint32_t>(MessageQueueFlagBits::NOT_FULL));
    }
    return Result::OK;
}

// ------- IStreamIn -------
Return<void> StreamInVirtualMic::getAudioSource(getAudioSource_cb _hidl_cb) {
    // AudioSource is a string typedef in audio@7.0.
    _hidl_cb(Result::NOT_SUPPORTED, hidl_string{});
    return Void();
}

Return<Result> StreamInVirtualMic::setGain(float /*gain*/) {
    return Result::NOT_SUPPORTED;
}

Return<Result> StreamInVirtualMic::updateSinkMetadata(const SinkMetadata& /*sinkMetadata*/) {
    return Result::OK;
}

Return<void> StreamInVirtualMic::prepareForReading(uint32_t frameSize, uint32_t framesCount,
                                                   prepareForReading_cb _hidl_cb) {
    std::lock_guard<std::mutex> lk(mLock);

    int32_t threadInfo = 0;
    auto sendError = [&threadInfo, &_hidl_cb](Result result) {
        _hidl_cb(result, CommandMQ::Descriptor(), DataMQ::Descriptor(),
                 StatusMQ::Descriptor(), threadInfo);
    };

    if (mDataMQ) {
        ALOGE("prepareForReading called twice");
        sendError(Result::INVALID_STATE);
        return Void();
    }

    if (frameSize == 0 || framesCount == 0) {
        ALOGE("Null frameSize (%u) or framesCount (%u)", frameSize, framesCount);
        sendError(Result::INVALID_ARGUMENTS);
        return Void();
    }
    if (frameSize > kMaxBufferBytes / framesCount) {
        ALOGE("Requested buffer too large: %u*%u", frameSize, framesCount);
        sendError(Result::INVALID_ARGUMENTS);
        return Void();
    }

    mFrameSizeBytes = frameSize;
    mFrameCount = framesCount;

    auto tempCommandMQ = std::make_unique<CommandMQ>(1);
    auto tempDataMQ =
            std::make_unique<DataMQ>(frameSize * framesCount, true /* EventFlag */);
    auto tempStatusMQ = std::make_unique<StatusMQ>(1);

    if (!tempCommandMQ->isValid() || !tempDataMQ->isValid() || !tempStatusMQ->isValid()) {
        ALOGE_IF(!tempCommandMQ->isValid(), "command MQ invalid");
        ALOGE_IF(!tempDataMQ->isValid(), "data MQ invalid");
        ALOGE_IF(!tempStatusMQ->isValid(), "status MQ invalid");
        sendError(Result::INVALID_ARGUMENTS);
        return Void();
    }

    EventFlag* tempRawEfGroup = nullptr;
    status_t status =
            EventFlag::createEventFlag(tempDataMQ->getEventFlagWord(), &tempRawEfGroup);
    if (status != ::android::OK || tempRawEfGroup == nullptr) {
        ALOGE("failed creating event flag for data MQ: %s", strerror(-status));
        sendError(Result::INVALID_ARGUMENTS);
        return Void();
    }

    auto tempReadThread = sp<ReadThread>::make(
            &mStopReadThread, mSource, tempCommandMQ.get(), tempDataMQ.get(),
            tempStatusMQ.get(), tempRawEfGroup, &mFramesRead,
            mConfig.base.sampleRateHz, mFrameSizeBytes);
    if (!tempReadThread->init()) {
        ALOGE("failed to init reader thread");
        EventFlag::deleteEventFlag(&tempRawEfGroup);
        sendError(Result::INVALID_ARGUMENTS);
        return Void();
    }
    status = tempReadThread->run("virtualmic-reader",
                                 ::android::PRIORITY_URGENT_AUDIO);
    if (status != ::android::OK) {
        ALOGE("failed to start reader thread: %s", strerror(-status));
        EventFlag::deleteEventFlag(&tempRawEfGroup);
        sendError(Result::INVALID_ARGUMENTS);
        return Void();
    }

    mCommandMQ = std::move(tempCommandMQ);
    mDataMQ = std::move(tempDataMQ);
    mStatusMQ = std::move(tempStatusMQ);
    mReadThread = tempReadThread;
    mEfGroup = tempRawEfGroup;

    threadInfo = mReadThread->getTid();
    _hidl_cb(Result::OK, *mCommandMQ->getDesc(), *mDataMQ->getDesc(),
             *mStatusMQ->getDesc(), threadInfo);
    return Void();
}

Return<uint32_t> StreamInVirtualMic::getInputFramesLost() {
    return 0;
}

Return<void> StreamInVirtualMic::getCapturePosition(getCapturePosition_cb _hidl_cb) {
    uint64_t frames = mFramesRead.load(std::memory_order_relaxed);
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t time =
            static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL +
            static_cast<uint64_t>(ts.tv_nsec);
    _hidl_cb(Result::OK, frames, time);
    return Void();
}

Return<void> StreamInVirtualMic::getActiveMicrophones(getActiveMicrophones_cb _hidl_cb) {
    _hidl_cb(Result::OK, hidl_vec<MicrophoneInfo>{});
    return Void();
}

Return<Result> StreamInVirtualMic::setMicrophoneDirection(MicrophoneDirection /*direction*/) {
    return Result::NOT_SUPPORTED;
}

Return<Result> StreamInVirtualMic::setMicrophoneFieldDimension(float /*zoom*/) {
    return Result::NOT_SUPPORTED;
}

}  // namespace virtualmic
}  // namespace implementation
}  // namespace V7_1
}  // namespace audio
}  // namespace hardware
}  // namespace android
