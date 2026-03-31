package com.example.vmictest

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.util.AttributeSet
import android.view.View

/**
 * Real-time waveform visualization view.
 * Shows audio samples as a scrolling waveform.
 */
class WaveformView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    private val wavePaint = Paint().apply {
        color = Color.parseColor("#00E676")  // Bright green
        strokeWidth = 3f
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

    private val path = Path()
    
    // Ring buffer of samples for display
    private val displaySamples = FloatArray(512)
    private var writeIndex = 0
    
    // For smooth animation
    private var targetSamples = FloatArray(512)
    
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
        postInvalidate()
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
        
        // Draw waveform
        path.reset()
        val sampleWidth = w / displaySamples.size
        
        for (i in displaySamples.indices) {
            val x = i * sampleWidth
            val y = centerY - displaySamples[i] * (h / 2) * 0.9f
            
            if (i == 0) {
                path.moveTo(x, y)
            } else {
                path.lineTo(x, y)
            }
        }
        
        canvas.drawPath(path, wavePaint)
    }
    
    fun setWaveColor(color: Int) {
        wavePaint.color = color
        postInvalidate()
    }
}
