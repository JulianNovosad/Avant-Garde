package com.wiredvideoviewer

import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.util.Log
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import org.json.JSONObject
import org.zeromq.SocketType
import org.zeromq.ZContext
import org.zeromq.ZMQ

class OrientationSender(
    private val scope: CoroutineScope,
    private val onUpdate: ((Float, Float, Float) -> Unit)? = null
) : SensorEventListener {
    private val TAG = "OrientationSender"
    private val PORT = 6001
    
    private var isRunning = false
    private var sendJob: Job? = null
    
    // Thread-safe vars for latest sensor data
    @Volatile private var latestYaw: Float = 0f
    @Volatile private var latestPitch: Float = 0f
    @Volatile private var latestRoll: Float = 0f
    
    private var context: ZContext? = null
    private var publisher: ZMQ.Socket? = null

    fun start() {
        if (isRunning) return
        isRunning = true
        
        Log.d(TAG, "Starting Orientation Sender on port $PORT")
        
        sendJob = scope.launch(Dispatchers.IO) {
            try {
                if (context == null) context = ZContext()
                publisher = context?.createSocket(SocketType.PUB)
                publisher?.bind("tcp://*:$PORT")
                
                Log.d(TAG, "ZeroMQ Publisher bound to tcp://*:$PORT")

                val sb = StringBuilder()
                while (isActive && isRunning) {
                    sb.setLength(0)
                    sb.append("{\"yaw\":").append(latestYaw)
                      .append(",\"pitch\":").append(latestPitch)
                      .append(",\"roll\":").append(latestRoll)
                      .append("}")
                    
                    publisher?.send(sb.toString())
                    
                    // 120Hz = ~8.33ms
                    delay(8) 
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error in OrientationSender loop: ${e.message}")
            } finally {
                cleanup()
            }
        }
    }

    fun stop() {
        isRunning = false
        sendJob?.cancel()
        // Cleanup happens in finally block or here if job wasn't running
        if (sendJob == null) cleanup()
    }

    private fun cleanup() {
        try {
            publisher?.close()
            publisher = null
            // Don't close context if we want to reuse? Better to close to be safe.
            context?.close()
            context = null
            Log.d(TAG, "Orientation Sender stopped")
        } catch (e: Exception) {
            Log.e(TAG, "Error cleaning up: ${e.message}")
        }
    }

    private var gravity: FloatArray? = null
    private var geomagnetic: FloatArray? = null

    override fun onSensorChanged(event: SensorEvent?) {
        event ?: return
        // Log.v(TAG, "Sensor event: ${event.sensor.type}") 
        
        if (event.sensor.type == Sensor.TYPE_ROTATION_VECTOR) {
            val rotationMatrix = FloatArray(9)
            val orientationAngles = FloatArray(3)
            
            android.hardware.SensorManager.getRotationMatrixFromVector(rotationMatrix, event.values)
            android.hardware.SensorManager.getOrientation(rotationMatrix, orientationAngles)
            
            val rawYaw = Math.toDegrees(orientationAngles[0].toDouble()).toFloat()
            val rawPitch = Math.toDegrees(orientationAngles[1].toDouble()).toFloat()
            val rawRoll = Math.toDegrees(orientationAngles[2].toDouble()).toFloat()
            
            latestYaw = -rawYaw
            latestPitch = rawRoll + 90f
            latestRoll = -rawPitch
            
            onUpdate?.invoke(latestYaw, latestPitch, latestRoll)
        } else if (event.sensor.type == Sensor.TYPE_ACCELEROMETER) {
            gravity = event.values.clone()
        } else if (event.sensor.type == Sensor.TYPE_MAGNETIC_FIELD) {
            geomagnetic = event.values.clone()
        }

        if (gravity != null && geomagnetic != null && event.sensor.type != Sensor.TYPE_ROTATION_VECTOR) {
            val r = FloatArray(9)
            val i = FloatArray(9)
            if (android.hardware.SensorManager.getRotationMatrix(r, i, gravity, geomagnetic)) {
                val orientation = FloatArray(3)
                android.hardware.SensorManager.getOrientation(r, orientation)
                
                val rawYaw = Math.toDegrees(orientation[0].toDouble()).toFloat()
                val rawPitch = Math.toDegrees(orientation[1].toDouble()).toFloat()
                val rawRoll = Math.toDegrees(orientation[2].toDouble()).toFloat()
                
                latestYaw = -rawYaw
                latestPitch = rawRoll + 90f
                latestRoll = -rawPitch
                
                onUpdate?.invoke(latestYaw, latestPitch, latestRoll)
            }
        }
    }

    override fun onAccuracyChanged(sensor: Sensor?, accuracy: Int) {}
}
