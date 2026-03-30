/**
 * Audio Generator - Generates test signals and writes to shared memory
 * 
 * Supports: Sine wave, frequency sweep, white noise, chirp
 */

#include <jni.h>
#include <android/log.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <chrono>
#include <atomic>

#include "virtual_mic_writer.h"

#define LOG_TAG "AudioGenerator"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum SignalType {
    SINE = 0,
    SWEEP = 1,
    NOISE = 2,
    CHIRP = 3
};

struct AudioGenerator {
    int sampleRate;
    int bufferSize;
    double phase = 0.0;
    double frequency = 440.0;
    SignalType signalType = SINE;
    
    // Sweep parameters
    double sweepMinFreq = 20.0;
    double sweepMaxFreq = 20000.0;
    double sweepDuration = 5.0;  // seconds
    double sweepPhase = 0.0;
    
    // Timing
    int64_t sampleCount = 0;
    
    // Shared memory writer
    vmic::VirtualMicWriter writer;
    
    // Output buffer
    std::vector<float> buffer;
    std::vector<int16_t> pcmBuffer;
};

static float generateSineSample(AudioGenerator* gen) {
    float sample = sinf(gen->phase);
    gen->phase += 2.0 * M_PI * gen->frequency / gen->sampleRate;
    if (gen->phase > 2.0 * M_PI) {
        gen->phase -= 2.0 * M_PI;
    }
    return sample;
}

static float generateSweepSample(AudioGenerator* gen) {
    // Linear frequency sweep
    double t = (double)gen->sampleCount / gen->sampleRate;
    double progress = fmod(t, gen->sweepDuration) / gen->sweepDuration;
    double freq = gen->sweepMinFreq + (gen->sweepMaxFreq - gen->sweepMinFreq) * progress;
    
    float sample = sinf(gen->sweepPhase);
    gen->sweepPhase += 2.0 * M_PI * freq / gen->sampleRate;
    if (gen->sweepPhase > 2.0 * M_PI) {
        gen->sweepPhase -= 2.0 * M_PI;
    }
    return sample;
}

static float generateNoiseSample(AudioGenerator* gen) {
    return ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
}

static float generateChirpSample(AudioGenerator* gen) {
    // Logarithmic frequency sweep (better for impulse response)
    double t = (double)gen->sampleCount / gen->sampleRate;
    double progress = fmod(t, gen->sweepDuration) / gen->sweepDuration;
    double freq = gen->sweepMinFreq * pow(gen->sweepMaxFreq / gen->sweepMinFreq, progress);
    
    float sample = sinf(gen->sweepPhase);
    gen->sweepPhase += 2.0 * M_PI * freq / gen->sampleRate;
    return sample;
}

extern "C" {

JNIEXPORT jlong JNICALL
Java_com_example_vmictest_MainActivity_nativeCreateGenerator(
        JNIEnv* env, jobject /* this */, jint sampleRate, jint bufferSize) {
    
    auto* gen = new AudioGenerator();
    gen->sampleRate = sampleRate;
    gen->bufferSize = bufferSize;
    gen->buffer.resize(bufferSize);
    gen->pcmBuffer.resize(bufferSize);
    
    // Initialize shared memory writer
    if (!gen->writer.initialize(sampleRate, 1, bufferSize)) {
        LOGE("Failed to initialize VirtualMicWriter");
        delete gen;
        return 0;
    }
    
    LOGI("AudioGenerator created: %d Hz, buffer %d", sampleRate, bufferSize);
    return reinterpret_cast<jlong>(gen);
}

JNIEXPORT jfloatArray JNICALL
Java_com_example_vmictest_MainActivity_nativeGenerateFrame(
        JNIEnv* env, jobject /* this */, jlong generatorPtr) {
    
    auto* gen = reinterpret_cast<AudioGenerator*>(generatorPtr);
    if (!gen) return nullptr;
    
    // Generate samples
    for (int i = 0; i < gen->bufferSize; i++) {
        float sample;
        switch (gen->signalType) {
            case SINE:
                sample = generateSineSample(gen);
                break;
            case SWEEP:
                sample = generateSweepSample(gen);
                break;
            case NOISE:
                sample = generateNoiseSample(gen);
                break;
            case CHIRP:
                sample = generateChirpSample(gen);
                break;
            default:
                sample = 0.0f;
        }
        
        // Apply slight amplitude reduction to avoid clipping
        sample *= 0.8f;
        
        gen->buffer[i] = sample;
        gen->pcmBuffer[i] = (int16_t)(sample * 32767.0f);
        gen->sampleCount++;
    }
    
    // Write to shared memory for HAL to read
    gen->writer.writeSamples(gen->pcmBuffer.data(), gen->bufferSize);
    
    // Return samples to Java for waveform display
    jfloatArray result = env->NewFloatArray(gen->bufferSize);
    env->SetFloatArrayRegion(result, 0, gen->bufferSize, gen->buffer.data());
    return result;
}

JNIEXPORT void JNICALL
Java_com_example_vmictest_MainActivity_nativeSetSignalType(
        JNIEnv* env, jobject /* this */, jlong generatorPtr, jint type) {
    
    auto* gen = reinterpret_cast<AudioGenerator*>(generatorPtr);
    if (gen) {
        gen->signalType = static_cast<SignalType>(type);
        gen->sweepPhase = 0.0;  // Reset sweep
        LOGI("Signal type set to %d", type);
    }
}

JNIEXPORT void JNICALL
Java_com_example_vmictest_MainActivity_nativeDestroyGenerator(
        JNIEnv* env, jobject /* this */, jlong generatorPtr) {
    
    auto* gen = reinterpret_cast<AudioGenerator*>(generatorPtr);
    if (gen) {
        gen->writer.shutdown();
        delete gen;
        LOGI("AudioGenerator destroyed");
    }
}

}  // extern "C"
