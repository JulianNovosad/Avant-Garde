package com.wiredvideoviewer

import android.content.Context
import android.content.pm.ActivityInfo
import android.content.pm.PackageManager
import android.hardware.Sensor
import android.hardware.SensorManager
import android.os.Bundle
import android.util.Log
import android.view.Choreographer
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.widget.Button
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.NetworkInterface

class MainActivity : AppCompatActivity(), SurfaceHolder.Callback {

    private lateinit var surfaceView: SurfaceView
    private lateinit var orientationData: TextView
    private lateinit var startButton: Button
    private lateinit var stopButton: Button
    private lateinit var systemStatus: TextView
    private lateinit var linkStatus: TextView
    private lateinit var asciiLogo: TextView
    private lateinit var logScrollView: ScrollView
    private lateinit var logTextView: TextView

    // Core Components
    private var piIpAddress: String? = null

    private var orientationSender: OrientationSender? = null
    private var mediaPlayer: android.media.MediaPlayer? = null
    
    // Sensor Manager for Orientation
    private lateinit var sensorManager: SensorManager
    private var rotationVectorSensor: Sensor? = null
    
    // Native Methods
    private external fun initDecoder(surface: Surface, width: Int, height: Int): Boolean
    private external fun releaseDecoder()
    private external fun startRtpReceiver(port: Int)
    private external fun stopRtpReceiver()
    private external fun setActivityReference(activity: MainActivity)
    
    private var frameCallback: Choreographer.FrameCallback? = null // Added

    companion object {
        private var logCallback: ((String) -> Unit)? = null

        @JvmStatic
        fun nativeLog(message: String) {
            logCallback?.invoke("NATIVE: $message")
        }

        init {
            try {
                System.loadLibrary("wiredvideoviewer-native")
            } catch (e: Exception) {
                Log.e("MainActivity", "Failed to load native library: ${e.message}")
            }
        }
        
        const val LOGO_ASCII = """
    _  _  _  ____   ___  ____  _____
   / \| ||  _ \ / _ \|  _ \| ____|
  / _ \ || |_) | | | | |_) |  _|
 / ___ \ ||  _ <| |_| |  _ <| |___
/_/   \_\_| \_\___/|_| \_\_____|
                                   
      __  __ _  __ __     __
     |  \/  | |/ / \ \   / /
     | |\/| | ' /   \ \ / / 
     | |  | | . \    \ V /  
     |_|  |_|_|\_\    \_/
"""
        const val VIDEO_RTP_PORT = 5000
    }

    private lateinit var scanningOverlay: android.view.View
    private lateinit var scanningText: TextView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // Enforce landscape orientation immediately
        requestedOrientation = android.content.pm.ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
        setContentView(R.layout.activity_main)

        // Set the log callback for native code
        logCallback = { message -> log(message) }

        surfaceView = findViewById(R.id.surfaceView)
        orientationData = findViewById(R.id.orientationData)
        startButton = findViewById(R.id.startButton)
        stopButton = findViewById(R.id.stopButton)
        systemStatus = findViewById(R.id.systemStatus)
        linkStatus = findViewById(R.id.linkStatus)
        asciiLogo = findViewById(R.id.asciiLogo)
        scanningOverlay = findViewById(R.id.scanningOverlay)
        scanningText = findViewById(R.id.scanningText)
        logScrollView = findViewById(R.id.logScrollView)
        logTextView = findViewById(R.id.logTextView)

        surfaceView.holder.addCallback(this)

        log("System nominal. Ready to initialize.")

        sensorManager = getSystemService(Context.SENSOR_SERVICE) as SensorManager
        rotationVectorSensor = sensorManager.getDefaultSensor(Sensor.TYPE_ROTATION_VECTOR)

        // Initialize Orientation Sender early for "always-on" telemetry
        orientationSender = OrientationSender(lifecycleScope) { y, p, r ->
            runOnUiThread {
                orientationData.text = "YAW: %.1f°\nPITCH: %.1f°\nROLL: %.1f°".format(y, p, r)
            }
        }

        startButton.setOnClickListener { handleStartButton() }
        stopButton.setOnClickListener { handleStopButton() }

        // Start Pi IP monitoring
        PiDiscovery.startMonitoringPi(this, lifecycleScope)
        
        // Observe changes in Pi IP address
        lifecycleScope.launch {
            PiDiscovery.piIpFlow.collect { newIp ->
                if (newIp != null && newIp != piIpAddress) {
                    log("Discovered new Pi IP: $newIp. Reconnecting...")
                    piIpAddress = newIp
                    if (startButton.text == "CONNECTED") { // Only restart if already connected
                        log("Stopping current streaming to reconnect to new IP.")
                        handleStopButton() // Stop current streaming gracefully
                        // Delay briefly to allow resources to release
                        delay(500) 
                        startConnectionProcess() // Re-initiate connection process with new IP
                    } else if (startButton.text == "RETRY" || startButton.text == "INITIALIZE") {
                        // If not connected yet, try to connect immediately
                        startConnectionProcess()
                    }
                } else if (newIp == null && piIpAddress != null) {
                    log("Pi IP lost. Disconnecting.")
                    handleStopButton() // Stop streaming if Pi is lost
                    systemStatus.text = "STATUS: OFFLINE"
                    startButton.text = "RETRY"
                    startButton.isEnabled = true
                    android.widget.Toast.makeText(this@MainActivity, "PI LOST", android.widget.Toast.LENGTH_LONG).show()
                }
            }
        }
    }

    private fun log(message: String) {
        // Ensure UI updates only happen if the activity is not finishing or destroyed
        if (isFinishing || isDestroyed) {
            Log.d("MainActivityLog", "Activity is finishing or destroyed, skipping UI log update: $message")
            return
        }
        runOnUiThread {
            Log.d("MainActivityLog", message)
            logTextView.append("$message\n")
            logScrollView.post { logScrollView.fullScroll(ScrollView.FOCUS_DOWN) }
        }
    }

    override fun onResume() {
        super.onResume()
        registerSensors()
        PiDiscovery.startMonitoringPi(this, lifecycleScope)
    }

    override fun onPause() {
        super.onPause()
        unregisterSensors()
        PiDiscovery.stopMonitoringPi()
    }

    private fun registerSensors() {
        orientationSender?.let { sender ->
            val sensorDelay = SensorManager.SENSOR_DELAY_UI
            if (rotationVectorSensor != null) {
                sensorManager.registerListener(sender, rotationVectorSensor, sensorDelay)
            } else {
                val accel = sensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER)
                val mag = sensorManager.getDefaultSensor(Sensor.TYPE_MAGNETIC_FIELD)
                sensorManager.registerListener(sender, accel, sensorDelay)
                sensorManager.registerListener(sender, mag, sensorDelay)
            }
        }
    }

    private fun unregisterSensors() {
        orientationSender?.let { sensorManager.unregisterListener(it) }
    }

    private fun handleStartButton() {
        log("Starting automatic discovery.")
        if (checkSelfPermission(android.Manifest.permission.ACCESS_FINE_LOCATION) != android.content.pm.PackageManager.PERMISSION_GRANTED) {
            requestPermissions(arrayOf(android.Manifest.permission.ACCESS_FINE_LOCATION), 101)
            return
        }
        startConnectionProcess()
    }



    private fun startConnectionProcess() {
        lifecycleScope.launch {
            startButton.isEnabled = false

            // ASCII Typing Effect - always show this when attempting connection
            asciiLogo.text = ""
            LOGO_ASCII.forEach { char ->
                asciiLogo.append(char.toString())
                if (char != ' ' && char != '\n') delay(5)
            }

            if (piIpAddress != null) {
                // Now that piIpAddress is set by the flow, proceed with connection
                scanningOverlay.visibility = android.view.View.GONE
                
                var connectionSuccessful = false
                var retryCount = 0
                val maxRetries = 5
                var delayTime = 1000L // 1 second initial delay

                while (!connectionSuccessful && retryCount < maxRetries) {
                    try {
                        // Then send START command to ControlModule
                        val myIp = getLocalIpAddress(piIpAddress!!) ?: "0.0.0.0"
                        log("Local IP determined as $myIp.")
                        log("Sending START command to $piIpAddress on port 6005... (Attempt ${retryCount + 1}/${maxRetries})")
                        ControlClient.sendStartCommand(piIpAddress!!, myIp, this@MainActivity::log)
                        connectionSuccessful = true
                    } catch (e: Exception) {
                        log("Connection attempt failed: ${e.message}")
                        retryCount++
                        if (retryCount < maxRetries) {
                            log("Retrying in ${delayTime / 1000} seconds...")
                            delay(delayTime)
                            delayTime *= 2 // Exponential back-off
                        }
                    }
                }

                if (connectionSuccessful) {
                    systemStatus.text = "STATUS: LINKED"
                    linkStatus.text = "AURORE MK V // Connected via Ethernet"
                    systemStatus.setTextColor(getColor(R.color.accent_red))
                    startButton.text = "CONNECTED"
                    startButton.isEnabled = false
                    log("Connection established with $piIpAddress. Starting streams.")
                    startStreaming()
                } else {
                    log("Failed to establish connection with $piIpAddress after $maxRetries attempts.")
                    systemStatus.text = "STATUS: OFFLINE"
                    startButton.text = "RETRY"
                    startButton.isEnabled = true
                    android.widget.Toast.makeText(this@MainActivity, "CONNECTION FAILED", android.widget.Toast.LENGTH_LONG).show()
                }
            } else {
                // If piIpAddress is null here, it means PiDiscovery hasn't found it yet
                // The piIpFlow observer will trigger startConnectionProcess when an IP is found
                log("Waiting for Pi IP discovery...")
                scanningOverlay.visibility = android.view.View.VISIBLE
                scanningText.text = "DISCOVERING..."
                startButton.text = "SCANNING"
                startButton.isEnabled = false
            }
        }
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == 101 && grantResults.isNotEmpty() && grantResults[0] == android.content.pm.PackageManager.PERMISSION_GRANTED) {
            log("Location permission granted. Retrying discovery.")
            startConnectionProcess()
        } else {
            log("Location permission denied. Cannot perform discovery.")
        }
    }

    private fun startStreaming() {
        if (piIpAddress == null) {
            log("Error: startStreaming called with null IP address.")
            return
        }
        
        // Switch to Native RTP Receiver
        log("Starting native RTP receiver on UDP port $VIDEO_RTP_PORT...")
        startRtpReceiver(VIDEO_RTP_PORT)

        log("Starting orientation data publisher on TCP port 6001...")
        orientationSender?.start()
    }

    private fun setupMediaPlayer(url: String) {
        runOnUiThread {
            try {
                if (mediaPlayer == null) {
                    mediaPlayer = android.media.MediaPlayer()
                    mediaPlayer?.setDisplay(surfaceView.holder)
                } else {
                    mediaPlayer?.reset()
                }
                
                mediaPlayer?.setDataSource(url)
                mediaPlayer?.prepareAsync()
                mediaPlayer?.setOnPreparedListener { 
                    it.start() 
                }
                mediaPlayer?.setOnErrorListener { _, what, extra ->
                    Log.e("MainActivity", "MediaPlayer Error: $what ($extra)")
                    log("MediaPlayer Error: $what ($extra)")
                    true
                }
            } catch (e: Exception) {
                Log.e("MainActivity", "Proxy Failure: ${e.message}")
                log("Proxy Failure: ${e.message}")
            }
        }
    }

    private fun handleStopButton() {
        log("Stop command initiated.")
        if (piIpAddress != null) {
            lifecycleScope.launch(Dispatchers.IO) {

                // Then send STOP command to ControlModule
                log("Sending STOP command to $piIpAddress on port 6005...")
                ControlClient.sendStopCommand(piIpAddress!!, this@MainActivity::log)
            }
        }
        
        log("Stopping native RTP receiver...")
        stopRtpReceiver()
        log("Stopping orientation sender...")
        orientationSender?.stop()
        
        PiDiscovery.stopMonitoringPi() // Ensure PiDiscovery also stops monitoring
        
        piIpAddress = null
        systemStatus.text = "STATUS: NOMINAL"
        linkStatus.text = "AURORE MK V // No connectivity"
        systemStatus.setTextColor(getColor(R.color.accent_red))
        startButton.isEnabled = true
        startButton.text = "INITIALIZE"
        log("System terminated and reset to nominal state.")
    }

    private fun getLocalIpAddress(targetIp: String): String? {
       try {
           val target = java.net.InetAddress.getByName(targetIp)
           val socket = java.net.DatagramSocket()
           // Use the control port (6005) for a more realistic dummy connect
           socket.connect(target, 6005) 
           val localIp = socket.localAddress.hostAddress
           socket.close()
           
           if (localIp != "0.0.0.0") return localIp
       } catch (e: Exception) { 
           Log.w("MainActivity", "Dummy connect failed for $targetIp, falling back to interface scan")
       }

       // Fallback: Manually find interface on same subnet
       try {
           val target = java.net.InetAddress.getByName(targetIp).address
           val interfaces = java.net.NetworkInterface.getNetworkInterfaces()
           while (interfaces.hasMoreElements()) {
               val iface = interfaces.nextElement()
               if (!iface.isUp || iface.isLoopback) continue
               
               for (ia in iface.interfaceAddresses) {
                   val addr = ia.address
                   if (addr is java.net.Inet4Address) {
                       val local = addr.address
                       // Simple check: do they share the same first 3 bytes (common for /24)
                       if (local[0] == target[0] && local[1] == target[1] && local[2] == target[2]) {
                           return addr.hostAddress
                       }
                   }
               }
           }
           
           // Extreme Fallback: Just return any non-loopback IPv4
           val interfaces2 = java.net.NetworkInterface.getNetworkInterfaces()
           while (interfaces2.hasMoreElements()) {
               val iface = interfaces2.nextElement()
               if (!iface.isUp || iface.isLoopback) continue
               for (addr in iface.inetAddresses) {
                   if (addr is java.net.Inet4Address) return addr.hostAddress
               }
           }
       } catch (e: Exception) {
           Log.e("MainActivity", "Total failure determining local IP: ${e.message}")
       }
       return null
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        val surfaceWidth = holder.surfaceFrame.width()
        val surfaceHeight = holder.surfaceFrame.height()
        Log.d("MainActivity", "Surface created: SurfaceView dimensions: ${surfaceView.width}x${surfaceView.height}, SurfaceHolder dimensions: ${surfaceWidth}x${surfaceHeight}")
        initDecoder(holder.surface, surfaceWidth, surfaceHeight)
        log("SurfaceView size = ${surfaceView.width} x ${surfaceView.height}")
        log("SurfaceHolder size = ${holder.surfaceFrame}")

        // Start continuous redraw
        frameCallback = object : Choreographer.FrameCallback {
            override fun doFrame(frameTimeNanos: Long) {
                surfaceView.invalidate() // Trigger redraw
                Choreographer.getInstance().postFrameCallback(this) // Post for next frame
            }
        }
        Choreographer.getInstance().postFrameCallback(frameCallback!!)
    }
    override fun surfaceChanged(holder: SurfaceHolder, format: Int, w: Int, h: Int) {
        initDecoder(holder.surface, w, h)
        log("SurfaceView size = ${surfaceView.width} x ${surfaceView.height}")
        log("SurfaceHolder size = ${holder.surfaceFrame}")
    }
    override fun surfaceDestroyed(holder: SurfaceHolder) {
        // Stop continuous redraw
        frameCallback?.let {
            Choreographer.getInstance().removeFrameCallback(it)
        }
        frameCallback = null

        releaseDecoder()
    }
}