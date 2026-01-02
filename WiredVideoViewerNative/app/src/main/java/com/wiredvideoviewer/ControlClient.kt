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

    fun sendStartCommand(piIp: String, myIp: String) {
        val commands = listOf(
            "START $myIp",
            "start $myIp",
            "{\"command\":\"start\",\"ip\":\"$myIp\"}",
            "{\"msg\":\"START\",\"ip\":\"$myIp\"}",
            "INIT $myIp",
            myIp,
            "START"
        )
        
        // Broad port range based on various docs and scans
        val ports = listOf(6000, 6005, 5000, 1001, 8554, 8080)

        CoroutineScope(Dispatchers.IO).launch {
            Log.d(TAG, "Initiating Aggressive Start for Pi at $piIp (Local: $myIp)")
            for (port in ports) {
                for (cmd in commands) {
                    sendSingleCommand(piIp, port, cmd)
                    delay(20) // Tight delay
                }
            }
        }
    }

    private fun sendSingleCommand(piIp: String, port: Int, command: String) {
        // Try TCP
        try {
            val socket = Socket()
            socket.setSoTimeout(150)
            socket.connect(java.net.InetSocketAddress(piIp, port), 150)
            val writer = PrintWriter(socket.getOutputStream(), true)
            writer.println(command)
            socket.close()
            Log.d(TAG, "TCP Success: '$command' to $piIp:$port")
            if (port == 6005 && command.startsWith("START")) {
                Log.i(TAG, "Explicitly sent START command to $piIp:6005 via TCP.")
            }
        } catch (e: Exception) { }

        // Try UDP
        try {
            val udpSocket = java.net.DatagramSocket()
            val data = (command + "\n").toByteArray()
            val packet = java.net.DatagramPacket(data, data.size, java.net.InetAddress.getByName(piIp), port)
            udpSocket.send(packet)
            udpSocket.close()
            // We don't log UDP success because it's connectionless
            if (port == 6005 && command.startsWith("START")) {
                Log.i(TAG, "Explicitly sent START command to $piIp:6005 via UDP (connectionless).")
            }
        } catch (e: Exception) { }
    }

    suspend fun sendStopCommand(piIp: String): Boolean = withContext(Dispatchers.IO) {
        val ports = listOf(6000, 5000, 1001)
        for (port in ports) {
            sendSingleCommand(piIp, port, "STOP")
        }
        true
    }
}
