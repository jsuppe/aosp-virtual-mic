package com.example.vmictest

import android.Manifest
import android.content.pm.PackageManager
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import android.os.Bundle
import android.util.Log
import android.widget.Button
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import kotlin.math.abs
import kotlin.math.sin

/**
 * Virtual Microphone Test App
 * 
 * Displays side-by-side waveforms:
 * - Left: Audio being sent to virtual mic (source)
 * - Right: Audio received from virtual mic (via AudioRecord)
 * 
 * If both match → pipeline works!
 */
class MainActivity : AppCompatActivity() {
    
    companion object {
        private const val TAG = "VMicTest"
        private const val REQUEST_RECORD_PERMISSION = 1
        private const val SAMPLE_RATE = 48000
        private const val BUFFER_SIZE = 512
    }
    
    // Views
    private lateinit var sourceWaveform: WaveformView
    private lateinit var micWaveform: WaveformView
    private lateinit var statusText: TextView
    private lateinit var btnSine: Button
    private lateinit var btnSweep: Button
    private lateinit var btnNoise: Button
    
    // Audio
    private var audioRecord: AudioRecord? = null
    private var isRecording = false
    private var isGenerating = false
    
    // Native
    private var nativeGenerator: Long = 0
    
    // Signal type
    private var signalType = SignalType.SINE
    private var frequency = 440.0
    
    enum class SignalType { SINE, SWEEP, NOISE, CHIRP }
    
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        
        sourceWaveform = findViewById(R.id.source_waveform)
        micWaveform = findViewById(R.id.mic_waveform)
        statusText = findViewById(R.id.status_text)
        btnSine = findViewById(R.id.btn_sine)
        btnSweep = findViewById(R.id.btn_sweep)
        btnNoise = findViewById(R.id.btn_noise)
        
        btnSine.setOnClickListener { setSignalType(SignalType.SINE) }
        btnSweep.setOnClickListener { setSignalType(SignalType.SWEEP) }
        btnNoise.setOnClickListener { setSignalType(SignalType.NOISE) }
        
        checkPermissionAndStart()
    }
    
    private fun setSignalType(type: SignalType) {
        signalType = type
        nativeSetSignalType(nativeGenerator, type.ordinal)
        updateStatus()
    }
    
    private fun checkPermissionAndStart() {
        if (checkSelfPermission(Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(
                this,
                arrayOf(Manifest.permission.RECORD_AUDIO),
                REQUEST_RECORD_PERMISSION
            )
        } else {
            startAudio()
        }
    }
    
    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == REQUEST_RECORD_PERMISSION && 
            grantResults.isNotEmpty() && 
            grantResults[0] == PackageManager.PERMISSION_GRANTED) {
            startAudio()
        } else {
            statusText.text = "Microphone permission denied"
        }
    }
    
    private fun startAudio() {
        // Initialize native audio generator
        nativeGenerator = nativeCreateGenerator(SAMPLE_RATE, BUFFER_SIZE)
        if (nativeGenerator == 0L) {
            statusText.text = "Failed to create audio generator"
            return
        }
        
        // Start generating audio to shared memory
        isGenerating = true
        Thread {
            while (isGenerating) {
                val samples = nativeGenerateFrame(nativeGenerator)
                if (samples != null) {
                    runOnUiThread {
                        sourceWaveform.updateSamples(samples)
                    }
                }
                Thread.sleep(10) // ~100 updates/sec
            }
        }.start()
        
        // Start recording from virtual microphone
        startRecording()
        
        updateStatus()
    }
    
    private fun startRecording() {
        val minBufferSize = AudioRecord.getMinBufferSize(
            SAMPLE_RATE,
            AudioFormat.CHANNEL_IN_MONO,
            AudioFormat.ENCODING_PCM_16BIT
        )
        
        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) 
            != PackageManager.PERMISSION_GRANTED) {
            return
        }
        
        // Try to find virtual mic (device 100) or use default
        audioRecord = AudioRecord(
            MediaRecorder.AudioSource.MIC,
            SAMPLE_RATE,
            AudioFormat.CHANNEL_IN_MONO,
            AudioFormat.ENCODING_PCM_16BIT,
            minBufferSize * 2
        )
        
        if (audioRecord?.state != AudioRecord.STATE_INITIALIZED) {
            Log.e(TAG, "AudioRecord failed to initialize")
            statusText.text = "Failed to open microphone"
            return
        }
        
        isRecording = true
        audioRecord?.startRecording()
        
        Thread {
            val buffer = ShortArray(BUFFER_SIZE)
            while (isRecording) {
                val read = audioRecord?.read(buffer, 0, BUFFER_SIZE) ?: 0
                if (read > 0) {
                    // Convert to float for waveform display
                    val floatBuffer = FloatArray(read) { buffer[it] / 32768f }
                    runOnUiThread {
                        micWaveform.updateSamples(floatBuffer)
                    }
                }
            }
        }.start()
    }
    
    private fun updateStatus() {
        val signalName = when (signalType) {
            SignalType.SINE -> "440Hz Sine"
            SignalType.SWEEP -> "Frequency Sweep"
            SignalType.NOISE -> "White Noise"
            SignalType.CHIRP -> "Chirp"
        }
        statusText.text = "$signalName | ${SAMPLE_RATE}Hz | Buffer: $BUFFER_SIZE"
    }
    
    override fun onDestroy() {
        super.onDestroy()
        isGenerating = false
        isRecording = false
        audioRecord?.stop()
        audioRecord?.release()
        if (nativeGenerator != 0L) {
            nativeDestroyGenerator(nativeGenerator)
        }
    }
    
    // Native methods
    private external fun nativeCreateGenerator(sampleRate: Int, bufferSize: Int): Long
    private external fun nativeGenerateFrame(generator: Long): FloatArray?
    private external fun nativeSetSignalType(generator: Long, type: Int)
    private external fun nativeDestroyGenerator(generator: Long)
    
    companion object {
        init {
            System.loadLibrary("vmic_unified_test")
        }
    }
}
