package com.example.vmictest

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.graphics.Color
import android.media.AudioDeviceInfo
import android.media.AudioFormat
import android.media.AudioManager
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
    private var isDemoMode = false
    private var currentDemoFreq = 440
    private lateinit var sendingText: TextView
    private lateinit var statusText: TextView
    private lateinit var frequencyText: TextView
    private lateinit var amplitudeText: TextView
    private lateinit var alignmentText: TextView
    private lateinit var waveformText: TextView
    private lateinit var waveformView: WaveformView
    private lateinit var levelMeter: ProgressBar
    private lateinit var recordButton: Button
    private lateinit var demoButton: Button
    private lateinit var latencyButton: Button
    
    // Latency measurement
    private var latencyTestActive = false
    private var impulseStartTime = 0L
    private var lastMeasuredLatency = 0.0
    private val latencyMeasurements = mutableListOf<Double>()
    
    // Demo frequencies to cycle through
    private val demoFrequencies = listOf(262, 330, 392, 440, 523, 659, 784, 880, 1047)
    private val noteNames = listOf("C4", "E4", "G4", "A4", "C5", "E5", "G5", "A5", "C6")
    
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        
        sendingText = findViewById(R.id.sendingText)
        statusText = findViewById(R.id.statusText)
        frequencyText = findViewById(R.id.frequencyText)
        amplitudeText = findViewById(R.id.amplitudeText)
        alignmentText = findViewById(R.id.alignmentText)
        waveformText = findViewById(R.id.waveformText)
        waveformView = findViewById(R.id.waveformView)
        levelMeter = findViewById(R.id.levelMeter)
        recordButton = findViewById(R.id.recordButton)
        demoButton = findViewById(R.id.demoButton)
        
        // Configure expected waveform overlay
        waveformView.setExpectedFrequency(440f)
        waveformView.setSampleRate(SAMPLE_RATE)
        waveformView.setShowExpected(true)
        
        recordButton.setOnClickListener {
            if (isRecording) {
                stopRecording()
            } else {
                checkPermissionAndRecord()
            }
        }
        
        demoButton.setOnClickListener {
            if (isDemoMode) {
                stopDemo()
            } else {
                startDemo()
            }
        }
        
        latencyButton = findViewById(R.id.latencyButton)
        latencyButton.setOnClickListener {
            runLatencyTest()
        }
        
        statusText.text = "Ready. Tap DEMO to cycle tones, or RECORD to listen."
        sendingText.text = "Sending: -- Hz"
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
        
        // Find virtual mic device (BUS type with address "virtual_mic_0")
        val audioManager = getSystemService(Context.AUDIO_SERVICE) as AudioManager
        val devices = audioManager.getDevices(AudioManager.GET_DEVICES_INPUTS)
        var virtualMicDevice: AudioDeviceInfo? = null
        
        Log.i(TAG, "Available input devices:")
        for (device in devices) {
            val typeName = when(device.type) {
                AudioDeviceInfo.TYPE_BUILTIN_MIC -> "BUILTIN_MIC"
                AudioDeviceInfo.TYPE_USB_DEVICE -> "USB_DEVICE"
                AudioDeviceInfo.TYPE_BUS -> "BUS"
                AudioDeviceInfo.TYPE_REMOTE_SUBMIX -> "REMOTE_SUBMIX"
                else -> "TYPE_${device.type}"
            }
            Log.i(TAG, "  Device: id=${device.id}, type=$typeName, " +
                       "address=${device.address}, name=${device.productName}")
            
            // Look for BUS device with our virtual mic address
            if (device.type == AudioDeviceInfo.TYPE_BUS && 
                device.address.contains("virtual_mic")) {
                virtualMicDevice = device
                Log.i(TAG, "  → Found virtual mic!")
            }
        }
        
        val audioSource = MediaRecorder.AudioSource.MIC
        Log.i(TAG, "Using audio source: $audioSource (MIC)")
        
        // Use AudioRecord.Builder with setPreferredDevice
        audioRecord = AudioRecord.Builder()
            .setAudioSource(audioSource)
            .setAudioFormat(AudioFormat.Builder()
                .setSampleRate(SAMPLE_RATE)
                .setChannelMask(CHANNEL_CONFIG)
                .setEncoding(AUDIO_FORMAT)
                .build())
            .setBufferSizeInBytes(bufferSize * 2)
            .build()
        
        // Set preferred device if found
        if (virtualMicDevice != null) {
            val result = audioRecord?.setPreferredDevice(virtualMicDevice)
            Log.i(TAG, "Set preferred device to virtual mic: $result")
            statusText.text = "Recording from virtual mic..."
        } else {
            Log.w(TAG, "Virtual mic device not found, using default")
            statusText.text = "Recording from mic (virtual not found)..."
        }
        
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
        
        // Detect impulse for latency measurement (peak > 90% of max)
        if (latencyTestActive && maxSample > 29000 && impulseStartTime > 0) {
            val detectedTime = System.nanoTime()
            val latencyNs = detectedTime - impulseStartTime
            val latencyMs = latencyNs / 1_000_000.0
            
            // Only count reasonable latencies (1-150ms)
            if (latencyMs > 0.5 && latencyMs < 150) {
                synchronized(latencyMeasurements) {
                    latencyMeasurements.add(latencyMs)
                }
                Log.i(TAG, "Impulse detected! Latency: %.2f ms, peak: $maxSample".format(latencyMs))
                impulseStartTime = 0  // Reset for next impulse
            }
        }
        
        runOnUiThread {
            frequencyText.text = "Freq: %.0f Hz".format(estimatedFreq)
            amplitudeText.text = "RMS: %.1f dB, Peak: %d".format(rmsDb, maxSample)
            levelMeter.progress = level
            
            // Update waveform
            waveformView.updateWaveform(monoSamples, monoSamples.size)
            
            // Update alignment metrics (fixed width to prevent flickering)
            val corrPercent = (waveformView.correlationScore * 100).toInt()
            alignmentText.text = "Corr: %3d%% | Phase: %+6.1f ms".format(
                corrPercent,
                waveformView.phaseOffsetMs
            )
            
            // Color based on correlation quality
            alignmentText.setTextColor(when {
                corrPercent >= 95 -> Color.parseColor("#00E676")  // Green - excellent
                corrPercent >= 80 -> Color.parseColor("#FFEB3B")  // Yellow - good
                else -> Color.parseColor("#F44336")  // Red - poor
            })
            
            // Color code frequency text based on expected 440Hz
            when {
                estimatedFreq > 420 && estimatedFreq < 460 -> {
                    // Green - close to 440Hz (match!)
                    frequencyText.setTextColor(Color.parseColor("#00E676"))
                }
                maxSample < 100 -> {
                    // Gray - silence
                    frequencyText.setTextColor(Color.parseColor("#888888"))
                }
                else -> {
                    // Cyan - different frequency (still valid audio)
                    frequencyText.setTextColor(Color.parseColor("#00BCD4"))
                }
            }
        }
    }
    
    private fun startDemo() {
        isDemoMode = true
        demoButton.text = "STOP DEMO"
        demoButton.backgroundTintList = android.content.res.ColorStateList.valueOf(Color.parseColor("#F44336"))
        
        // Disable (not hide) record button during demo
        recordButton.isEnabled = false
        recordButton.alpha = 0.5f
        
        // Start recording if not already
        if (!isRecording) {
            checkPermissionAndRecord()
        }
        
        // Start demo cycle
        thread {
            var freqIndex = 0
            while (isDemoMode) {
                val freq = demoFrequencies[freqIndex]
                val note = noteNames[freqIndex]
                
                runOnUiThread {
                    sendingText.text = "Sending: $freq Hz ($note)"
                    waveformView.setExpectedFrequency(freq.toFloat())
                }
                
                // Start renderer with this frequency
                startRenderer(freq)
                
                // Wait 3 seconds before switching
                Thread.sleep(3000)
                
                // Next frequency
                freqIndex = (freqIndex + 1) % demoFrequencies.size
            }
        }
    }
    
    private fun stopDemo() {
        isDemoMode = false
        demoButton.text = "DEMO"
        demoButton.backgroundTintList = android.content.res.ColorStateList.valueOf(Color.parseColor("#FF9800"))
        sendingText.text = "Sending: -- Hz"
        
        // Re-enable record button
        recordButton.isEnabled = true
        recordButton.alpha = 1.0f
        
        // Stop recording too
        if (isRecording) {
            stopRecording()
        }
        
        // Kill renderer and reset flag
        rendererStarted = false
        thread {
            try {
                Runtime.getRuntime().exec(arrayOf("sh", "-c", "pkill virtual_mic_renderer"))
                Runtime.getRuntime().exec(arrayOf("sh", "-c", "rm -f /data/local/tmp/vmic_freq"))
            } catch (e: Exception) {
                Log.e(TAG, "Failed to stop renderer: ${e.message}")
            }
        }
    }
    
    private var rendererStarted = false
    
    private fun runLatencyTest() {
        // Start recording if not already
        if (!isRecording) {
            checkPermissionAndRecord()
        }
        
        // Use synchronized list for thread safety
        latencyMeasurements.clear()
        latencyTestActive = true
        
        runOnUiThread {
            sendingText.text = "Latency Test: Starting..."
        }
        
        thread {
            Thread.sleep(1000)  // Wait for recording to stabilize
            
            // Run 10 impulse tests
            for (i in 1..10) {
                runOnUiThread {
                    sendingText.text = "Latency Test: $i/10"
                }
                
                // Record timestamp and send impulse
                impulseStartTime = System.nanoTime()
                try {
                    Runtime.getRuntime().exec(arrayOf("sh", "-c", "echo IMPULSE > /data/local/tmp/vmic_freq"))
                    Log.i(TAG, "Impulse $i sent at ${impulseStartTime}")
                } catch (e: Exception) {
                    Log.e(TAG, "Failed to send impulse: ${e.message}")
                }
                
                // Wait for detection
                Thread.sleep(200)
            }
            
            // Wait a bit more for last detections
            Thread.sleep(300)
            latencyTestActive = false
            
            // Calculate results
            val measurements = latencyMeasurements.toList()
            runOnUiThread {
                if (measurements.isNotEmpty()) {
                    val sorted = measurements.sorted()
                    // Use median to ignore outliers
                    val median = sorted[sorted.size / 2]
                    val min = sorted.first()
                    val max = sorted.last()
                    sendingText.text = "Latency: %.1f ms (range %.1f-%.1f)".format(median, min, max)
                    statusText.text = "Measured ${measurements.size}/10 impulses"
                    Log.i(TAG, "Latency results: median=${median}ms, min=${min}ms, max=${max}ms, count=${measurements.size}")
                } else {
                    sendingText.text = "Latency Test: No impulses detected"
                    statusText.text = "Check renderer is running"
                }
            }
        }
    }
    
    private fun startRenderer(freqHz: Int) {
        thread {
            try {
                // Write frequency to control file (dynamic update)
                Runtime.getRuntime().exec(arrayOf("sh", "-c", "echo $freqHz > /data/local/tmp/vmic_freq"))
                Log.i(TAG, "Set frequency: $freqHz Hz")
                
                // Start renderer only once (it reads freq from control file)
                if (!rendererStarted) {
                    Runtime.getRuntime().exec(arrayOf("sh", "-c", "pkill virtual_mic_renderer")).waitFor()
                    Thread.sleep(100)
                    val cmd = "/data/local/tmp/virtual_mic_renderer $freqHz 0.5 0"  // 0 = infinite
                    Log.i(TAG, "Starting renderer: $cmd")
                    Runtime.getRuntime().exec(arrayOf("sh", "-c", "nohup $cmd > /dev/null 2>&1 &"))
                    rendererStarted = true
                    Thread.sleep(500)  // Wait for renderer to connect
                }
            } catch (e: Exception) {
                Log.e(TAG, "Failed to set frequency: ${e.message}")
            }
        }
    }
    
    override fun onDestroy() {
        super.onDestroy()
        stopDemo()
        stopRecording()
    }
}

