// NativeAudio.cpp (JNI glue)
#include <jni.h>
#include <memory>
#include "AudioEngine.h"

static std::unique_ptr<AudioEngine> engine;

extern "C" {

JNIEXPORT void JNICALL
Java_com_example_voiceprocessor_1oboe_NativeAudio_init(JNIEnv* /*env*/, jobject /*thiz*/, jint /*sr*/, jint /*bs*/) {
    if (!engine) {
        engine = std::make_unique<AudioEngine>();
        engine->init(false, SAMPLE_RATE, BUFFER_SIZE);
    }
}

JNIEXPORT void JNICALL
Java_com_example_voiceprocessor_1oboe_NativeAudio_startBypass(JNIEnv* /*env*/, jobject /*thiz*/) {
    if (engine) {
        engine->setGain(1.0f);
        engine->start();
    }
}

JNIEXPORT void JNICALL
Java_com_example_voiceprocessor_1oboe_NativeAudio_startAmplification(JNIEnv* /*env*/, jobject /*thiz*/) {
    if (engine) {
        engine->setGain(2.0f);
        engine->start();
    }
}

JNIEXPORT void JNICALL
Java_com_example_voiceprocessor_1oboe_NativeAudio_stopAudio(JNIEnv* /*env*/, jobject /*thiz*/) {
    if (engine) {
        engine->stop();
    }
}

JNIEXPORT jshortArray JNICALL
Java_com_example_voiceprocessor_1oboe_NativeAudio_getRecordedData(JNIEnv* env, jobject /*thiz*/) {
    auto data = engine ? engine->getRecordedData() : std::vector<int16_t>();
    jshortArray arr = env->NewShortArray(data.size());
    env->SetShortArrayRegion(arr, 0, data.size(), data.data());
    return arr;
}

JNIEXPORT jint JNICALL
Java_com_example_voiceprocessor_1oboe_NativeAudio_getSampleRate(JNIEnv* /*env*/, jobject /*thiz*/) {
    return engine ? engine->getSampleRate() : SAMPLE_RATE;
}

JNIEXPORT void JNICALL
Java_com_example_voiceprocessor_1oboe_NativeAudio_setGain(JNIEnv* /*env*/, jobject /*thiz*/, jfloat gain) {
    if (engine) {
        engine->setGain(gain);
    }
}

} // extern "C"
