// NativeAudio.kt
package com.example.voiceprocessor_oboe

class NativeAudio {
    companion object {
        init { System.loadLibrary("voiceprocessor_oboe") }
    }
    external fun init(sampleRate: Int, bufferSize: Int)
    external fun startBypass()
    external fun startAmplification()
    external fun stopAudio()
    external fun getRecordedData(): ShortArray
    external fun getSampleRate(): Int
    external fun setGain(gain: Float)
}
