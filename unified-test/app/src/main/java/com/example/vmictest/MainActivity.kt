package com.example.vmictest

import android.Manifest
import android.content.pm.PackageManager
import android.graphics.Color
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import android.os.Bundle
import android.util.Log
import android.widget.Button
import android.widget.ProgressBar
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import kotlin.concurrent.thread
import kotlin.math.abs
import kotlin.math.sqrt

class MainActivity : AppCompatActivity() {
    
    companion object {
        private const val TAG = "VirtualMicTest"
        private const val SAMPLE_RATE = 48000
        private const val CHANNEL_CONFIG = AudioFormat.CHANNEL_IN_STEREO
        private const val AUDIO_FORMAT = AudioFormat.ENCODING_PCM_16BIT
        private const val PERMISSION_REQUEST_CODE = 123
    }
    
    private var audioRecord: AudioRecord? = null
    private var isRecording = false
    private lateinit var statusText: TextView
    private lateinit var frequencyText: TextView
    private lateinit var amplitudeText: TextView
    private lateinit var waveformText: TextView
    private lateinit var waveformView: WaveformView
    private lateinit var levelMeter: ProgressBar
    private lateinit var recordButton: Button
    
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        
        statusText = findViewById(R.id.statusText)
        frequencyText = findViewById(R.id.frequencyText)
        amplitudeText = findViewById(R.id.amplitudeText)
        waveformText = findViewById(R.id.waveformText)
        waveformView = findViewById(R.id.waveformView)
        levelMeter = findViewById(R.id.levelMeter)
        recordButton = findViewById(R.id.recordButton)
        
        recordButton.setOnClickListener {
            if (isRecording) {
                stopRecording()
            } else {
                checkPermissionAndRecord()
            }
        }
        
        statusText.text = "Ready. Run virtual_mic_renderer, then tap Record."
    }
    
    private fun checkPermissionAndRecord() {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) 
            != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(
                this, 
                arrayOf(Manifest.permission.RECORD_AUDIO), 
                PERMISSION_REQUEST_CODE
            )
        } else {
            startRecording()
        }
    }
    
    override fun onRequestPermissionsResult(
        requestCode: Int, 
        permissions: Array<out String>, 
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == PERMISSION_REQUEST_CODE && 
            grantResults.isNotEmpty() && 
            grantResults[0] == PackageManager.PERMISSION_GRANTED) {
            startRecording()
        }
    }
    
    private fun startRecording() {
        val bufferSize = AudioRecord.getMinBufferSize(SAMPLE_RATE, CHANNEL_CONFIG, AUDIO_FORMAT)
        Log.i(TAG, "Buffer size: $bufferSize")
        
        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) 
            != PackageManager.PERMISSION_GRANTED) {
            statusText.text = "Permission denied"
            return
        }
        
        val audioSource = MediaRecorder.AudioSource.MIC
        Log.i(TAG, "Using audio source: $audioSource (MIC)")
        
        audioRecord = AudioRecord(
            audioSource,
            SAMPLE_RATE,
            CHANNEL_CONFIG,
            AUDIO_FORMAT,
            bufferSize * 2
        )
        
        if (audioRecord?.state != AudioRecord.STATE_INITIALIZED) {
            statusText.text = "Failed to initialize AudioRecord"
            Log.e(TAG, "AudioRecord not initialized")
            return
        }
        
        isRecording = true
        recordButton.text = "STOP"
        recordButton.backgroundTintList = android.content.res.ColorStateList.valueOf(Color.parseColor("#F44336"))
        statusText.text = "Recording from mic..."
        
        audioRecord?.startRecording()
        
        thread {
            val buffer = ShortArray(4096)
            var frameCount = 0
            val startTime = System.currentTimeMillis()
            
            while (isRecording) {
                val read = audioRecord?.read(buffer, 0, buffer.size) ?: 0
                if (read > 0) {
                    frameCount += read
                    analyzeAudio(buffer, read)
                }
            }
            
            val duration = (System.currentTimeMillis() - startTime) / 1000.0
            Log.i(TAG, "Recorded $frameCount frames in $duration seconds")
        }
    }
    
    private fun stopRecording() {
        isRecording = false
        audioRecord?.stop()
        audioRecord?.release()
        audioRecord = null
        recordButton.text = "RECORD"
        recordButton.backgroundTintList = android.content.res.ColorStateList.valueOf(Color.parseColor("#4CAF50"))
        statusText.text = "Stopped"
    }
    
    private fun analyzeAudio(buffer: ShortArray, length: Int) {
        // Extract left channel only for analysis
        val monoSamples = ShortArray(length / 2)
        for (i in monoSamples.indices) {
            monoSamples[i] = buffer[i * 2]
        }
        
        // Calculate RMS amplitude
        var sumSquares = 0.0
        var maxSample = 0
        for (i in monoSamples.indices) {
            val sample = abs(monoSamples[i].toInt())
            sumSquares += sample * sample
            if (sample > maxSample) maxSample = sample
        }
        val rms = sqrt(sumSquares / monoSamples.size)
        val rmsDb = 20 * kotlin.math.log10(rms / 32767.0 + 1e-10)
        
        // Zero-crossing rate for frequency estimation
        var zeroCrossings = 0
        for (i in 1 until monoSamples.size) {
            val prev = monoSamples[i-1]
            val curr = monoSamples[i]
            if ((prev >= 0 && curr < 0) || (prev < 0 && curr >= 0)) {
                zeroCrossings++
            }
        }
        val durationSec = monoSamples.size.toDouble() / SAMPLE_RATE
        val estimatedFreq = if (durationSec > 0) zeroCrossings / durationSec / 2 else 0.0
        
        // Level meter (0-100)
        val level = (maxSample * 100 / 32767).coerceIn(0, 100)
        
        runOnUiThread {
            frequencyText.text = "Freq: %.0f Hz".format(estimatedFreq)
            amplitudeText.text = "RMS: %.1f dB, Peak: %d".format(rmsDb, maxSample)
            levelMeter.progress = level
            
            // Update waveform
            waveformView.updateWaveform(monoSamples, monoSamples.size)
            
            // Color code based on expected 440Hz
            when {
                estimatedFreq > 420 && estimatedFreq < 460 -> {
                    // Green - close to 440Hz
                    frequencyText.setTextColor(Color.parseColor("#00E676"))
                    waveformView.setWaveColor(Color.parseColor("#00E676"))
                }
                maxSample < 100 -> {
                    // Gray - silence
                    frequencyText.setTextColor(Color.parseColor("#888888"))
                    waveformView.setWaveColor(Color.parseColor("#444444"))
                }
                else -> {
                    // Cyan - different frequency (still valid audio)
                    frequencyText.setTextColor(Color.parseColor("#00BCD4"))
                    waveformView.setWaveColor(Color.parseColor("#00BCD4"))
                }
            }
        }
    }
    
    override fun onDestroy() {
        super.onDestroy()
        stopRecording()
    }
}
