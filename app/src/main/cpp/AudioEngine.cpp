// AudioEngine.cpp
#include "AudioEngine.h"
#include <android/log.h>
#include <algorithm>

#define LOG_TAG "AudioEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Constructor: 초기 샘플레이트, 버퍼 크기, 녹음 총 프레임 수 설정 및 내부 벡터 예약
AudioEngine::AudioEngine()
        : mSampleRate(SAMPLE_RATE), // 오디오 스트림 샘플레이트
          mBufferSize(BUFFER_SIZE), // 콜백 당 프레임 수
          mTotalFrames(SAMPLE_RATE * 10) // 약 10초 분량의 녹음 저장 공간
{
    mRecordedLeft.reserve(mTotalFrames);   // 녹음된 왼쪽 채널 저장 벡터
    mRecordedRight.reserve(mTotalFrames);  // 녹음된 오른쪽 채널 저장 벡터
}

// Destructor: 스트림이 켜져 있으면 중지 및 닫기
AudioEngine::~AudioEngine() {
    stop();  // 안전하게 스트림 중지
}
// init(): 스트림 한 번만 초기화, 내부 상태 및 버퍼 리셋
oboe::Result AudioEngine::init(bool /*amplificationMode*/, int32_t sampleRate, int32_t bufferSize) {
    // 초기값 세팅
    mGain        = 1.0f;          // 증폭 배율 기본값
    mSampleRate  = sampleRate;     // JNI에서 전달받은 샘플레이트
    mBufferSize  = bufferSize;     // JNI에서 전달받은 버퍼 크기
    mTotalFrames = mSampleRate * 10;
    mIsStarted   = false;          // start/stop 가드 플래그 초기화

    // 콜백 중첩 방지를 위해 이전 버퍼 비우기
    mRoutingBuffer.clear();        // 라우팅용 큐
    mRecordedLeft.clear();         // 저장용 왼쪽 채널
    mRecordedRight.clear();        // 저장용 오른쪽 채널

    // OUTPUT STREAM 생성 및 설정
    {
        oboe::AudioStreamBuilder builder;
        builder.setDirection(oboe::Direction::Output) // 출력용 스트림
                ->setPerformanceMode(oboe::PerformanceMode::LowLatency) // 낮은 지연 모드
                ->setSharingMode(oboe::SharingMode::Exclusive) // 독점 모드
                ->setSampleRate(0) // 0이면 하드웨어 기본값
                //->setFramesPerCallback(48)
                ->setChannelCount(1) // 모노 출력
                ->setFormat(oboe::AudioFormat::I16) // 16-bit PCM
                ->setCallback(this); // 콜백 객체 등록
        auto result = builder.openManagedStream(mOutputStream);
        if (result != oboe::Result::OK) {
            LOGE("Output open failed: %s", oboe::convertToText(result));
            return result;  // 실패 시 에러 반환
        }
    }

    // INPUT STREAM 생성 및 설정 (마이크 입력)
    {
        oboe::AudioStreamBuilder builder;
        builder.setDirection(oboe::Direction::Input) // 입력용 스트림
                ->setPerformanceMode(oboe::PerformanceMode::LowLatency) // 낮은 지연 모드
                ->setSharingMode(oboe::SharingMode::Exclusive) // 독점 모드
                ->setSampleRate(0) // 0이면 하드웨어 기본값
                //->setFramesPerCallback(48)
                ->setChannelCount(1) // 모노 입력
                ->setFormat(oboe::AudioFormat::I16) // 16-bit PCM
                ->setCallback(this); // 콜백 객체 등록
        auto result = builder.openManagedStream(mInputStream);
        if (result != oboe::Result::OK) {
            LOGE("Input open failed: %s", oboe::convertToText(result));
            return result;  // 실패 시 에러 반환
        }
    }

    // 실제 사용 샘플레이트 로깅
    mSampleRate = mInputStream->getSampleRate();
    LOGI("Using sample rate: %d", mSampleRate);
    return oboe::Result::OK;
}

// start(): 스트림 시작, 중복 호출 시 무시
oboe::Result AudioEngine::start() {
    if (mIsStarted) return oboe::Result::OK; // 이미 시작된 경우 패스

    auto r1 = mOutputStream->requestStart(); // 출력 스트림 시작 요청
    if (r1 != oboe::Result::OK) {
        LOGE("Start output failed: %s", oboe::convertToText(r1));
        return r1;
    }
    auto r2 = mInputStream->requestStart(); // 입력 스트림 시작 요청
    if (r2 != oboe::Result::OK) {
        LOGE("Start input failed: %s", oboe::convertToText(r2));
        return r2;
    }
    mIsStarted = true; // 성공 시 플래그 세팅
    return oboe::Result::OK;
}
// stop(): 스트림 중지 및 닫기, 중복 호출 시 무시
oboe::Result AudioEngine::stop() {
    if (!mIsStarted) return oboe::Result::OK; // 시작 안 된 경우 패스

    if (mInputStream) {
        mInputStream->requestStop();
        mInputStream->close();
    }
    if (mOutputStream) {
        mOutputStream->requestStop();
        mOutputStream->close();
    }
    mIsStarted = false; // 중지 후 플래그 리셋
    return oboe::Result::OK;
}
// onAudioReady(): 입력/출력 콜백 핸들링
oboe::DataCallbackResult AudioEngine::onAudioReady(oboe::AudioStream* stream, void* audioData, int32_t numFrames) {
    // 입력 스트림 처리: mRoutingBuffer에 푸시, 녹음 데이터 저장
    if (stream == mInputStream.get()) {
        auto* inBuf = static_cast<int16_t*>(audioData);
        for (int i = 0; i < numFrames; ++i) {
            // 증폭 및 클램핑
            int32_t sample = static_cast<int32_t>(inBuf[i] * mGain);
            sample = std::clamp(sample, -32768, 32767);
            int16_t s = static_cast<int16_t>(sample);
            mRoutingBuffer.push_back(s); // 라우팅 큐에 추가
            // 녹음 버퍼에 지정된 총 프레임 수까지만 저장
            if ((int)mRecordedLeft.size() < mTotalFrames) {
                mRecordedLeft.push_back(s);
                mRecordedRight.push_back(s);
            }
        }
        return oboe::DataCallbackResult::Continue;
    }
    // 출력 스트림 처리: mRoutingBuffer에서 읽어와 출력 버퍼에 복사
    auto* outBuf = static_cast<int16_t*>(audioData);
    for (int i = 0; i < numFrames; ++i) {
        int16_t s = 0;
        if (!mRoutingBuffer.empty()) {
            s = mRoutingBuffer.front(); // 큐 앞의 샘플
            mRoutingBuffer.pop_front(); // 큐에서 제거
        }
        outBuf[i] = s;  // 출력
    }
    return oboe::DataCallbackResult::Continue;
}

// getRecordedData(): 녹음된 좌/우 채널을 인터리브해 반환
std::vector<int16_t> AudioEngine::getRecordedData() {
    size_t frames = std::min(mRecordedLeft.size(), mRecordedRight.size());
    std::vector<int16_t> stereo;
    stereo.reserve(frames * 2);
    for (size_t i = 0; i < frames; ++i) {
        stereo.push_back(mRecordedLeft[i]);
        stereo.push_back(mRecordedRight[i]);
    }
    return stereo;
}

// setGain(): 외부에서 증폭 배율 조정, 0~4 사이로 클램핑
void AudioEngine::setGain(float gain) {
    mGain = std::clamp(gain, 0.0f, 4.0f);
    LOGI("setGain: %.2f", mGain);
}