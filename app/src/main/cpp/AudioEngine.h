// AudioEngine.h
#pragma once
#include "AudioConfig.h"
#include <oboe/Oboe.h>
#include <vector>
#include <deque>
#include <memory>

// Oboe AudioStream을 소멸자와 함께 관리하기 위한 typedef
using ManagedStream = std::unique_ptr<oboe::AudioStream, oboe::StreamDeleterFunctor>;

// AudioEngine 클래스: oboe::AudioStreamCallback을 상속 받아 입력/출력 스트림을 처리
class AudioEngine : public oboe::AudioStreamCallback {
public:
    AudioEngine();   // 생성자: 내부 버퍼 및 상태 초기화
    ~AudioEngine();  // 소멸자: 실행 중인 스트림 stop()

    /**
     * 스트림 생성 및 내부 버퍼 초기화
     * @param amplificationMode unused 현재는 gain으로 컨트롤
     * @param sampleRate 샘플레이트 (기본 SAMPLE_RATE)
     * @param bufferSize 콜백 당 처리할 프레임 수 (기본 BUFFER_SIZE)
     */
    oboe::Result init(bool amplificationMode,
                      int32_t sampleRate = SAMPLE_RATE,
                      int32_t bufferSize  = BUFFER_SIZE);

    /**
     * 스트림 start 요청
     * 중복 호출 시 두 번째 이후는 무시되어 재시작 방지
     */
    oboe::Result start();
    /**
     * 스트림 stop 요청 및 close
     * 중복 호출 시 두 번째 이후는 무시
     */
    oboe::Result stop();

    /**
     * 녹음된 데이터를 좌/우 채널로 인터리브하여 반환
     * @return interleaved stereo PCM data
     */
    std::vector<int16_t> getRecordedData();

    /**
     * 현재 사용 중인 샘플레이트 반환
     */
    int getSampleRate() const { return mSampleRate; }

    /**
     * 오디오 콜백 함수
     * @param stream onAudioReady를 호출한 스트림 포인터
     * @param audioData 입력 또는 출력 버퍼
     * @param numFrames 프레임 수
     */
    oboe::DataCallbackResult onAudioReady(oboe::AudioStream *stream,
                                          void *audioData,
                                          int32_t numFrames) override;

    /**
     * 증폭 비율 설정 (0.0 ~ 4.0)
     * 내부 버퍼는 초기화하지 않고 gain만 변경
     */
    void setGain(float gain);

private:
    ManagedStream mInputStream;    // 마이크 입력 스트림 핸들러
    ManagedStream mOutputStream;   // 스피커 출력 스트림 핸들러

    float mGain = 1.0f;  // 증폭 배율 (기본 1.0)
    bool mIsStarted = false; // start/stop 상태 가드 플래그

    std::deque<int16_t> mRoutingBuffer; // 입력 → 출력 라우팅용 큐
    std::vector<int16_t> mRecordedLeft; // 녹음용 좌채널 버퍼
    std::vector<int16_t> mRecordedRight; // 녹음용 우채널 버퍼

    int32_t mSampleRate; // 스트림 샘플레이트
    int32_t mBufferSize; // 콜백 당 처리할 프레임 수
    int32_t mTotalFrames; // 녹음 저장 총 프레임 수 (예: 샘플레이트 × 10초)
};
