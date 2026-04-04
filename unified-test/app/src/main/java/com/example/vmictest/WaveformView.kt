package com.example.vmictest

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.util.AttributeSet
import android.view.View
import kotlin.math.PI
import kotlin.math.sin

/**
 * Real-time waveform visualization view.
 * Shows audio samples as a scrolling waveform with optional expected overlay.
 */
class WaveformView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    // Actual captured waveform (cyan)
    private val actualPaint = Paint().apply {
        color = Color.parseColor("#00BCD4")  // Cyan
        strokeWidth = 3f
        style = Paint.Style.STROKE
        isAntiAlias = true
    }
    
    // Expected waveform (green, semi-transparent)
    private val expectedPaint = Paint().apply {
        color = Color.parseColor("#8000E676")  // Semi-transparent green
        strokeWidth = 4f
        style = Paint.Style.STROKE
        isAntiAlias = true
    }
    
    private val centerLinePaint = Paint().apply {
        color = Color.parseColor("#444444")
        strokeWidth = 1f
        style = Paint.Style.STROKE
    }
    
    private val gridPaint = Paint().apply {
        color = Color.parseColor("#333333")
        strokeWidth = 1f
        style = Paint.Style.STROKE
    }
    
    private val labelPaint = Paint().apply {
        color = Color.parseColor("#888888")
        textSize = 28f
        isAntiAlias = true
    }

    private val actualPath = Path()
    private val expectedPath = Path()
    
    // Ring buffer of samples for display
    private val displaySamples = FloatArray(512)
    private val expectedSamples = FloatArray(512)
    
    // Expected frequency for overlay generation
    private var expectedFreqHz = 440f
    private var sampleRate = 48000
    private var sampleOffset = 0L  // Track phase for continuous wave
    private var showExpected = true
    
    // Alignment metrics
    var correlationScore = 0f
        private set
    var phaseOffsetSamples = 0
        private set
    var phaseOffsetMs = 0f
        private set
    
    fun updateWaveform(samples: ShortArray, length: Int) {
        // Downsample to fit display buffer
        val step = maxOf(1, length / displaySamples.size)
        var j = 0
        for (i in 0 until length step step) {
            if (j < displaySamples.size) {
                // Normalize to -1..1
                displaySamples[j] = samples[i] / 32768f
                j++
            }
        }
        
        // Generate expected waveform (440Hz sine)
        if (showExpected) {
            val samplesPerCycle = sampleRate / expectedFreqHz
            for (i in expectedSamples.indices) {
                val sampleIndex = sampleOffset + (i * step)
                val phase = (sampleIndex % samplesPerCycle) / samplesPerCycle * 2 * PI
                expectedSamples[i] = (sin(phase) * 0.5).toFloat()  // 0.5 amplitude to match renderer
            }
            sampleOffset += length
            
            // Calculate cross-correlation to find best alignment
            calculateAlignment()
        }
        
        postInvalidate()
    }
    
    fun setExpectedFrequency(freqHz: Float) {
        expectedFreqHz = freqHz
    }
    
    fun setSampleRate(rate: Int) {
        sampleRate = rate
    }
    
    fun setShowExpected(show: Boolean) {
        showExpected = show
        postInvalidate()
    }
    
    /**
     * Calculate cross-correlation between expected and actual waveforms
     * to measure alignment (phase offset) and correlation score.
     */
    private fun calculateAlignment() {
        val maxLag = minOf(50, displaySamples.size / 4)  // Search +/- 50 samples
        var bestCorr = -1f
        var bestLag = 0
        
        // Normalize both signals
        val actualMean = displaySamples.average().toFloat()
        val expectedMean = expectedSamples.average().toFloat()
        
        var actualVar = 0f
        var expectedVar = 0f
        for (i in displaySamples.indices) {
            actualVar += (displaySamples[i] - actualMean) * (displaySamples[i] - actualMean)
            expectedVar += (expectedSamples[i] - expectedMean) * (expectedSamples[i] - expectedMean)
        }
        val actualStd = kotlin.math.sqrt(actualVar / displaySamples.size)
        val expectedStd = kotlin.math.sqrt(expectedVar / expectedSamples.size)
        
        if (actualStd < 0.001f || expectedStd < 0.001f) {
            // Signal too weak
            correlationScore = 0f
            phaseOffsetSamples = 0
            phaseOffsetMs = 0f
            return
        }
        
        // Try different lags
        for (lag in -maxLag..maxLag) {
            var corr = 0f
            var count = 0
            for (i in displaySamples.indices) {
                val j = i + lag
                if (j >= 0 && j < expectedSamples.size) {
                    corr += (displaySamples[i] - actualMean) * (expectedSamples[j] - expectedMean)
                    count++
                }
            }
            if (count > 0) {
                corr /= (count * actualStd * expectedStd)
                if (corr > bestCorr) {
                    bestCorr = corr
                    bestLag = lag
                }
            }
        }
        
        correlationScore = bestCorr.coerceIn(0f, 1f)
        phaseOffsetSamples = bestLag
        // Convert to ms (accounting for downsampling)
        val step = maxOf(1, 4096 / displaySamples.size)  // Approximate original step
        phaseOffsetMs = (bestLag * step * 1000f) / sampleRate
    }
    
    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        
        val w = width.toFloat()
        val h = height.toFloat()
        val centerY = h / 2
        
        // Draw grid lines
        for (i in 1..3) {
            val y = h * i / 4
            canvas.drawLine(0f, y, w, y, gridPaint)
        }
        
        // Draw center line
        canvas.drawLine(0f, centerY, w, centerY, centerLinePaint)
        
        val sampleWidth = w / displaySamples.size
        
        // Draw expected waveform first (behind)
        if (showExpected) {
            expectedPath.reset()
            for (i in expectedSamples.indices) {
                val x = i * sampleWidth
                val y = centerY - expectedSamples[i] * (h / 2) * 0.9f
                
                if (i == 0) {
                    expectedPath.moveTo(x, y)
                } else {
                    expectedPath.lineTo(x, y)
                }
            }
            canvas.drawPath(expectedPath, expectedPaint)
        }
        
        // Draw actual waveform (front)
        actualPath.reset()
        for (i in displaySamples.indices) {
            val x = i * sampleWidth
            val y = centerY - displaySamples[i] * (h / 2) * 0.9f
            
            if (i == 0) {
                actualPath.moveTo(x, y)
            } else {
                actualPath.lineTo(x, y)
            }
        }
        canvas.drawPath(actualPath, actualPaint)
        
        // Draw legend
        canvas.drawText("Expected (green)", 10f, 30f, expectedPaint.apply { style = Paint.Style.FILL })
        expectedPaint.style = Paint.Style.STROKE
        canvas.drawText("Actual (cyan)", 10f, 60f, actualPaint.apply { style = Paint.Style.FILL })
        actualPaint.style = Paint.Style.STROKE
    }
}
