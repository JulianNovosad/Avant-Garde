package com.wiredvideoviewer

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.util.Log
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.widget.Button
import android.widget.TextView
import android.widget.Spinner
import android.widget.ArrayAdapter
import android.widget.AdapterView
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.content.Context
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.NetworkInterface
import java.net.SocketException
import java.nio.ByteBuffer

class MainActivity : AppCompatActivity(), SurfaceHolder.Callback, SensorEventListener {

    private lateinit var surfaceView: SurfaceView
    private lateinit var orientationData: TextView
    private lateinit var startButton: Button
    private lateinit var stopButton: Button
    private lateinit var deviceSpinner: Spinner
    
    private lateinit var sensorManager: SensorManager
    private var rotationVectorSensor: Sensor? = null
    
    private var yaw: Float = 0.0f
    private var pitch: Float = 0.0f
    private var roll: Float = 0.0f
    
    // UDP socket for sending orientation data
    private var udpSocket: DatagramSocket? = null
    private var piIpAddress: String = "10.17.49.122" // Pi IP from config.json
    
    // Device discovery
    private lateinit var deviceList: MutableList<String>
    private lateinit var deviceAdapter: ArrayAdapter<String>
    
    // Flag to track if this is the first time clicking Start Stream
    private var isFirstStartClick = true
    
    // Port constants (matching the native code)
    companion object {
        const val YAW_PHONE_TO_PI_PORT = 2001
        const val PITCH_PHONE_TO_PI_PORT = 2002
        const val ROLL_PHONE_TO_PI_PORT = 2003
        
        init {
            System.loadLibrary("wiredvideoviewer-native")
        }

        @JvmStatic
        external fun initDecoder(surface: Surface): Boolean
        @JvmStatic
        external fun startRtpReceiver(port: Int)
        @JvmStatic
        external fun stopRtpReceiver()
        @JvmStatic
        external fun releaseDecoder()
        @JvmStatic
        external fun setActivityReference(activity: MainActivity)
    }
    
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        surfaceView = findViewById(R.id.surfaceView)
        orientationData = findViewById(R.id.orientationData)
        startButton = findViewById(R.id.startButton)
        stopButton = findViewById(R.id.stopButton)
        deviceSpinner = findViewById(R.id.deviceSpinner)

        surfaceView.holder.addCallback(this)

        // Initialize sensor manager and rotation vector sensor
        sensorManager = getSystemService(Context.SENSOR_SERVICE) as SensorManager
        rotationVectorSensor = sensorManager.getDefaultSensor(Sensor.TYPE_ROTATION_VECTOR)
        
        // Initialize device list and adapter
        deviceList = mutableListOf("10.17.49.122 (Default Pi)")
        deviceAdapter = ArrayAdapter(this, android.R.layout.simple_spinner_item, deviceList)
        deviceAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        deviceSpinner.adapter = deviceAdapter
        
        // Set listener for spinner selection
        deviceSpinner.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(parent: AdapterView<*>, view: android.view.View?, position: Int, id: Long) {
                val selectedDevice = deviceList[position]
                // Extract IP address from the selected item (everything before the space)
                piIpAddress = selectedDevice.split(" ")[0]
                Log.d("MainActivity", "Selected device IP: $piIpAddress")
            }
            
            override fun onNothingSelected(parent: AdapterView<*>) {
                // Use default IP if nothing is selected
                piIpAddress = "10.17.49.122"
            }
        }
        
        // Discover devices on the network
        discoverDevicesOnNetwork()

        startButton.setOnClickListener {
            if (isFirstStartClick) {
                // Show the device selection dialog for the first time
                showDeviceSelectionDialog()
                isFirstStartClick = false
            } else {
                // Directly start streaming with the previously selected IP
                startStreaming()
            }
        }

        stopButton.setOnClickListener {
            stopRtpReceiver()
            releaseDecoder()
            Log.d("MainActivity", "RTP receiver stopped and decoder released.")
        }
    }
    
    // Method to show device selection dialog
    private fun showDeviceSelectionDialog() {
        // Refresh device list
        discoverDevicesOnNetwork()
        
        // Create and show device selection dialog
        val builder = android.app.AlertDialog.Builder(this)
        builder.setTitle("Select Device to Connect")
        
        // Prepare device names for display (without IPs)
        val deviceNames = deviceList.map { 
            if (it.contains(" ")) {
                it.split(" ")[1] + " (" + it.split(" ")[0] + ")"
            } else {
                it
            }
        }.toTypedArray()
        
        builder.setItems(deviceNames) { _, which ->
            val selectedDevice = deviceList[which]
            // Extract IP address from the selected item (everything before the space)
            piIpAddress = if (selectedDevice.contains(" ")) {
                selectedDevice.split(" ")[0]
            } else {
                selectedDevice
            }
            Log.d("MainActivity", "Selected device IP: $piIpAddress")
            
            // Start streaming after device selection
            startStreaming()
        }
        
        builder.setCancelable(true)
        builder.show()
    }
    
    // Method to start streaming
    private fun startStreaming() {
        Log.d("MainActivity", "Starting streaming process...")
        // Hardcoded port for RTP video stream
        val port = 1001
        Log.d("MainActivity", "Initializing decoder with surface: ${surfaceView.holder.surface}")
        val decoderInitialized = initDecoder(surfaceView.holder.surface)
        if (decoderInitialized) {
            Log.d("MainActivity", "Decoder initialized successfully, starting RTP receiver on port $port")
            startRtpReceiver(port)
            Log.d("MainActivity", "Successfully started RTP receiver on port $port, connecting to $piIpAddress")
        } else {
            Log.e("MainActivity", "Failed to initialize decoder.")
        }
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        Log.d("MainActivity", "Surface created.")
        // initDecoder is called when the start button is pressed,
        // so we don't call it here.
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        Log.d("MainActivity", "Surface changed: width=$width, height=$height")
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        Log.d("MainActivity", "Surface destroyed, releasing decoder.")
        releaseDecoder()
    }

    // Method to discover devices on the network
    private fun discoverDevicesOnNetwork() {
        Thread {
            try {
                // Clear the device list and add the default device
                runOnUiThread {
                    deviceList.clear()
                    deviceList.add("10.17.49.122 (Default Pi)")
                    deviceAdapter.notifyDataSetChanged()
                }
                
                // Get the wired network interface (usually eth0 or similar)
                val networkInterfaces = NetworkInterface.getNetworkInterfaces()
                var wiredIpAddress: String? = null
                
                while (networkInterfaces.hasMoreElements()) {
                    val networkInterface = networkInterfaces.nextElement()
                    
                    // Skip loopback and wireless interfaces
                    if (networkInterface.isLoopback || networkInterface.displayName.contains("wlan") || 
                        networkInterface.displayName.contains("wifi") || networkInterface.displayName.contains("ap")) {
                        continue
                    }
                    
                    // Get the IP address of the wired interface
                    val addresses = networkInterface.inetAddresses
                    while (addresses.hasMoreElements()) {
                        val address = addresses.nextElement()
                        if (!address.isLoopbackAddress && address.hostAddress.indexOf(':') == -1) {
                            // Found a wired IPv4 address
                            wiredIpAddress = address.hostAddress
                            Log.d("MainActivity", "Found wired network IP: $wiredIpAddress")
                            break
                        }
                    }
                }
                
                // If we found a wired IP, scan for devices on the same subnet
                if (wiredIpAddress != null) {
                    val subnet = wiredIpAddress.substring(0, wiredIpAddress.lastIndexOf('.') + 1)
                    Log.d("MainActivity", "Scanning subnet: $subnet")
                    
                    // Scan common IP addresses on the subnet
                    for (i in 1..254) {
                        val targetIp = "$subnet$i"
                        if (targetIp != wiredIpAddress) {
                            // Try to ping the device
                            try {
                                val inetAddress = InetAddress.getByName(targetIp)
                                if (inetAddress.isReachable(1000)) { // 1 second timeout
                                    runOnUiThread {
                                        if (!deviceList.contains("$targetIp (Discovered)")) {
                                            deviceList.add("$targetIp (Discovered)")
                                            deviceAdapter.notifyDataSetChanged()
                                        }
                                    }
                                    Log.d("MainActivity", "Found device at: $targetIp")
                                }
                            } catch (e: Exception) {
                                // Device not reachable or other error, continue scanning
                            }
                        }
                    }
                }
            } catch (e: Exception) {
                Log.e("MainActivity", "Error discovering devices: ${e.message}")
            }
        }.start()
    }
    
    // Method to send orientation data to Pi via UDP
    private fun sendOrientationDataToPi(yaw: Float, pitch: Float, roll: Float) {
        Thread {
            try {
                if (udpSocket == null) {
                    udpSocket = DatagramSocket()
                }
                
                // Send yaw to port 2001
                val yawBytes = ByteBuffer.allocate(4).putFloat(yaw).array()
                val yawPacket = DatagramPacket(yawBytes, yawBytes.size, InetAddress.getByName(piIpAddress), YAW_PHONE_TO_PI_PORT)
                udpSocket?.send(yawPacket)
                
                // Send pitch to port 2002
                val pitchBytes = ByteBuffer.allocate(4).putFloat(pitch).array()
                val pitchPacket = DatagramPacket(pitchBytes, pitchBytes.size, InetAddress.getByName(piIpAddress), PITCH_PHONE_TO_PI_PORT)
                udpSocket?.send(pitchPacket)
                
                // Send roll to port 2003
                val rollBytes = ByteBuffer.allocate(4).putFloat(roll).array()
                val rollPacket = DatagramPacket(rollBytes, rollBytes.size, InetAddress.getByName(piIpAddress), ROLL_PHONE_TO_PI_PORT)
                udpSocket?.send(rollPacket)
                
                Log.d("MainActivity", "Sent orientation data - Yaw: $yaw, Pitch: $pitch, Roll: $roll")
            } catch (e: Exception) {
                Log.e("MainActivity", "Error sending orientation data: ${e.message}")
            }
        }.start()
    }

    // Method to update orientation data display
    fun updateOrientationData(yaw: Float, pitch: Float, roll: Float) {
        runOnUiThread {
            orientationData.text = "Yaw: %.1f°\nPitch: %.1f°\nRoll: %.1f°".format(yaw, pitch, roll)
        }
    }
    
    // Sensor event listener methods
    override fun onSensorChanged(event: SensorEvent?) {
        event?.also { sensorEvent ->
            if (sensorEvent.sensor.type == Sensor.TYPE_ROTATION_VECTOR) {
                val rotationMatrix = FloatArray(9)
                val orientationAngles = FloatArray(3)
                
                SensorManager.getRotationMatrixFromVector(rotationMatrix, sensorEvent.values)
                SensorManager.getOrientation(rotationMatrix, orientationAngles)
                
                // Convert from radians to degrees
                val rawYaw = Math.toDegrees(orientationAngles[0].toDouble()).toFloat()
                val rawPitch = Math.toDegrees(orientationAngles[1].toDouble()).toFloat()
                val rawRoll = Math.toDegrees(orientationAngles[2].toDouble()).toFloat()
                
                // Adjust for landscape orientation:
                // When phone is in landscape mode:
                // - Phone's roll of -90° should be treated as pitch 0°
                // - Phone's pitch should be treated as -1 * roll
                // - Phone's yaw remains as yaw (but inverted since we want clockwise to be positive)
                yaw = -rawYaw  // Invert yaw so clockwise is positive
                pitch = rawRoll + 90.0f  // Adjust roll to pitch with offset
                roll = -rawPitch  // Convert pitch to roll with inversion
                
                // Normalize angles to -180 to 180 range
                if (yaw > 180) yaw -= 360
                if (yaw < -180) yaw += 360
                if (pitch > 180) pitch -= 360
                if (pitch < -180) pitch += 360
                if (roll > 180) roll -= 360
                if (roll < -180) roll += 360
                
                // Update UI
                updateOrientationData(rawYaw, rawPitch, rawRoll)
                
                // Send adjusted orientation data to Pi
                sendOrientationDataToPi(yaw, pitch, roll)
            }
        }
    }
    
    override fun onAccuracyChanged(sensor: Sensor?, accuracy: Int) {
        // Not used in this implementation
    }
    
    override fun onResume() {
        super.onResume()
        setActivityReference(this)
        
        // Register sensor listener
        rotationVectorSensor?.also { sensor ->
            sensorManager.registerListener(this, sensor, SensorManager.SENSOR_DELAY_NORMAL)
        }
    }
    
    override fun onPause() {
        super.onPause()
        // Unregister sensor listener
        sensorManager.unregisterListener(this)
        
        // Close UDP socket
        udpSocket?.close()
        udpSocket = null
    }
}