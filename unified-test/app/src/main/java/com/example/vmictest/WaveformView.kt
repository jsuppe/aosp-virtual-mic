package com.example.vmictest

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.util.AttributeSet
import android.view.View
import kotlin.math.abs

/**
 * Custom view for displaying audio waveforms in real-time.
 */
class WaveformView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {
    
    private val waveformPaint = Paint().apply {
        color = Color.GREEN
        strokeWidth = 2f
        style = Paint.Style.STROKE
        isAntiAlias = true
    }
    
    private val gridPaint = Paint().apply {
        color = Color.argb(80, 255, 255, 255)
        strokeWidth = 1f
        style = Paint.Style.STROKE
    }
    
    private val centerPaint = Paint().apply {
        color = Color.argb(120, 255, 255, 255)
        strokeWidth = 1f
        style = Paint.Style.STROKE
    }
    
    private val peakPaint = Paint().apply {
        color = Color.YELLOW
        textSize = 32f
        isAntiAlias = true
    }
    
    private val path = Path()
    private var samples = FloatArray(512)
    private var peakLevel = 0f
    private var peakDecay = 0.95f
    
    // Ring buffer for scrolling waveform
    private val historySize = 2048
    private val history = FloatArray(historySize)
    private var historyIndex = 0
    
    fun updateSamples(newSamples: FloatArray) {
        // Add to history buffer
        for (sample in newSamples) {
            history[historyIndex] = sample
            historyIndex = (historyIndex + 1) % historySize
            
            // Track peak
            val absSample = abs(sample)
            if (absSample > peakLevel) {
                peakLevel = absSample
            }
        }
        
        // Decay peak
        peakLevel *= peakDecay
        
        invalidate()
    }
    
    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        
        val w = width.toFloat()
        val h = height.toFloat()
        val centerY = h / 2
        
        // Draw background
        canvas.drawColor(Color.parseColor("#1a1a1a"))
        
        // Draw grid lines
        for (i in 1..3) {
            val y = h * i / 4
            canvas.drawLine(0f, y, w, y, gridPaint)
        }
        for (i in 1..7) {
            val x = w * i / 8
            canvas.drawLine(x, 0f, x, h, gridPaint)
        }
        
        // Draw center line
        canvas.drawLine(0f, centerY, w, centerY, centerPaint)
        
        // Draw waveform from history
        path.reset()
        val samplesPerPixel = historySize / w
        var firstPoint = true
        
        for (px in 0 until w.toInt()) {
            // Find sample for this pixel
            val sampleIdx = ((historyIndex - historySize + px * samplesPerPixel).toInt() + historySize) % historySize
            val sample = history[sampleIdx]
            
            val x = px.toFloat()
            val y = centerY - sample * (h / 2 - 10)
            
            if (firstPoint) {
                path.moveTo(x, y)
                firstPoint = false
            } else {
                path.lineTo(x, y)
            }
        }
        
        canvas.drawPath(path, waveformPaint)
        
        // Draw peak meter
        val peakHeight = peakLevel * (h / 2 - 10)
        canvas.drawRect(w - 20, centerY - peakHeight, w - 5, centerY + peakHeight, waveformPaint)
        
        // Draw peak level text
        val peakDb = if (peakLevel > 0) (20 * kotlin.math.log10(peakLevel.toDouble())).toInt() else -60
        canvas.drawText("${peakDb}dB", w - 80, 30f, peakPaint)
    }
    
    fun setColor(color: Int) {
        waveformPaint.color = color
        invalidate()
    }
}
