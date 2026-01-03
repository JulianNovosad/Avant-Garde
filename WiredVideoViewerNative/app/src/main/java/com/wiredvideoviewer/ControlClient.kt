package com.wiredvideoviewer

import android.util.Log
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.PrintWriter
import java.net.Socket

object ControlClient {
    private const val TAG = "ControlClient"
    private const val PI_CONTROL_PORT = 6005

    fun sendStartCommand(piIp: String, myIp: String, log: (String) -> Unit) {
        CoroutineScope(Dispatchers.IO).launch {
            val command = "START $myIp"
            log("Attempting to send control command: '$command' to $piIp:$PI_CONTROL_PORT via TCP")
            sendSingleCommand(piIp, PI_CONTROL_PORT, command, log)
        }
    }

    private fun sendSingleCommand(piIp: String, port: Int, command: String, log: (String) -> Unit) {
        // Try TCP
        try {
            val socket = Socket()
            socket.setSoTimeout(5000) // 5 second timeout for connection and read
            socket.connect(java.net.InetSocketAddress(piIp, port), 5000)
            val writer = PrintWriter(socket.getOutputStream(), true)
            writer.println(command)
            socket.close()
            log("TCP Success: '$command' sent to $piIp:$port")
            Log.d(TAG, "TCP Success: '$command' sent to $piIp:$port")
        } catch (e: Exception) {
            log("TCP Failed: '$command' to $piIp:$port - ${e.message}")
            Log.e(TAG, "TCP Failed: '$command' to $piIp:$port - ${e.message}")
        }
    }

    suspend fun sendStopCommand(piIp: String, log: (String) -> Unit): Boolean = withContext(Dispatchers.IO) {
        val command = "STOP"
        val port = PI_CONTROL_PORT // Assuming STOP uses the same control port
        log("Attempting to send control command: '$command' to $piIp:$port via TCP")
        sendSingleCommand(piIp, port, command, log)
        true
    }
}
