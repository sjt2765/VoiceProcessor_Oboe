// MainActivity.kt
package com.example.voiceprocessor_oboe

import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import android.widget.Button
import android.widget.SeekBar
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import java.io.File
import java.io.FileOutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder

// MainActivity: 앱의 UI 제어 및 네이티브 오디오 엔진 호출을 담당
class MainActivity : AppCompatActivity() {
    // 권한 요청 시 사용하는 요청 코드
    private val PERMISSION_REQUEST_CODE = 1001
    // 네이티브 라이브러리와 연동하는 객체
    private val nativeAudio = NativeAudio()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // activity_main.xml 레이아웃을 화면에 설정
        setContentView(R.layout.activity_main)

        // RECORD_AUDIO 와 WRITE_EXTERNAL_STORAGE 권한이 없으면 요청
        if (!hasPermissions()) {
            ActivityCompat.requestPermissions(
                this,
                arrayOf(Manifest.permission.RECORD_AUDIO,
                    Manifest.permission.WRITE_EXTERNAL_STORAGE),
                PERMISSION_REQUEST_CODE
            )
        }

        // 네이티브 오디오 엔진 초기화: init()은 앱 실행 중 한 번만 호출
        nativeAudio.init(0, 0)

        // SeekBar: 증폭 비율 조절 UI
        findViewById<SeekBar>(R.id.seekBarAmplification).apply {
            max = 300           // 최대 300(=3.0배)
            progress = 100      // 초기값 100(=1.0배)
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                // 사용자가 슬라이더를 움직일 때마다 호출
                override fun onProgressChanged(sb: SeekBar, p: Int, fromUser: Boolean) {
                    // 0~300 값을 0.0f~3.0f 비율로 변환하여 네이티브에 전달
                    nativeAudio.setGain(p / 100.0f)
                }
                override fun onStartTrackingTouch(sb: SeekBar) {} // 터치 시작
                override fun onStopTrackingTouch(sb: SeekBar) {} // 터치 끝
            })
        }

        // Bypass 버튼: 증폭 없이 입력을 바로 출력
        findViewById<Button>(R.id.buttonBypass).setOnClickListener {
            Toast.makeText(this, "Bypass 모드 시작", Toast.LENGTH_SHORT).show()
            nativeAudio.startBypass()
        }

        // Amplification 버튼: 2× 증폭 모드 시작
        findViewById<Button>(R.id.buttonAmplification).setOnClickListener {
            Toast.makeText(this, "2× 증폭 모드 시작", Toast.LENGTH_SHORT).show()
            nativeAudio.startAmplification()
        }

        // Stop & Save 버튼: 녹음 중지 후 WAV 파일로 저장
        findViewById<Button>(R.id.buttonStopSave).setOnClickListener {
            Toast.makeText(this, "녹음 종료 및 저장", Toast.LENGTH_SHORT).show()
            nativeAudio.stopAudio()
            val recorded = nativeAudio.getRecordedData()  // ShortArray 반환
            val sr       = nativeAudio.getSampleRate()   // 샘플레이트
            saveWavFile(recorded, sr)
        }
    }

    // 권한이 모두 허용되었는지 확인
    private fun hasPermissions(): Boolean {
        val r = ContextCompat.checkSelfPermission(
            this, Manifest.permission.RECORD_AUDIO)
        val w = ContextCompat.checkSelfPermission(
            this, Manifest.permission.WRITE_EXTERNAL_STORAGE)
        return r == PackageManager.PERMISSION_GRANTED &&
                w == PackageManager.PERMISSION_GRANTED
    }

    /**
     * ShortArray 음성 데이터를 WAV 파일로 저장
     * @param audioData PCM 16비트 샘플 배열
     * @param sampleRate 파일 헤더에 기록할 샘플레이트
     */
    private fun saveWavFile(audioData: ShortArray, sampleRate: Int) {
        // WAV 포맷 헤더 설정 변수
        val numChannels   = 2  // stereo
        val bitsPerSample = 16 // 16-bit
        val dataSize = audioData.size * 2  // 바이트 수
        val chunkSize = 36 + dataSize       // RIFF 청크 크기
        val byteRate = sampleRate * numChannels * bitsPerSample / 8

        try {
            // 앱 전용 폴더 내 VoiceProcessor 디렉토리
            val folder = File(getExternalFilesDir(null), "VoiceProcessor")
            if (!folder.exists() && !folder.mkdirs()) {
                Toast.makeText(this, "폴더 생성 실패", Toast.LENGTH_SHORT).show()
                return
            }
            val file = File(folder, "recorded_Signal.wav")

            FileOutputStream(file).use { out ->
                // 44바이트 WAV 헤더 작성
                val header = ByteBuffer.allocate(44)
                    .order(ByteOrder.LITTLE_ENDIAN)
                header.put("RIFF".toByteArray()) // ChunkID
                    .putInt(chunkSize) // ChunkSize
                    .put("WAVE".toByteArray()) // Format
                    .put("fmt ".toByteArray()) // Subchunk1ID
                    .putInt(16) // Subchunk1Size (PCM)
                    .putShort(1.toShort()) // AudioFormat (PCM)
                    .putShort(numChannels.toShort())
                    .putInt(sampleRate)
                    .putInt(byteRate)
                    .putShort((numChannels * bitsPerSample / 8).toShort())
                    .putShort(bitsPerSample.toShort())
                    .put("data".toByteArray()) // Subchunk2ID
                    .putInt(dataSize) // Subchunk2Size
                out.write(header.array()) // 헤더 쓰기

                // PCM 데이터 쓰기
                val buf = ByteBuffer.allocate(dataSize)
                    .order(ByteOrder.LITTLE_ENDIAN)
                audioData.forEach { buf.putShort(it) }
                out.write(buf.array()) // 오디오 데이터
            }
            Toast.makeText(this, "파일 저장: ${file.absolutePath}", Toast.LENGTH_SHORT).show()
        } catch (e: Exception) {
            // 저장 실패 시 오류 메시지 표시
            Toast.makeText(this, "저장 실패: ${e.message}", Toast.LENGTH_SHORT).show()
        }
    }
}
