package com.wiredvideoviewer

import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.InputStream
import java.io.OutputStream
import java.net.InetAddress
import java.net.ServerSocket
import java.net.Socket

class VideoProxy(private val piIp: String, private val piPort: Int = 5000) {
    private val TAG = "VideoProxy"
    private var serverSocket: ServerSocket? = null
    var proxyPort: Int = 0
        private set
    
    private var isRunning = false

    fun start() {
        if (isRunning) return
        isRunning = true
        
        Thread {
            try {
                // Bind to localhost on random port
                serverSocket = ServerSocket(0, 0, InetAddress.getByName("127.0.0.1"))
                proxyPort = serverSocket?.localPort ?: 0
                Log.d(TAG, "Proxy started on 127.0.0.1:$proxyPort, targeting $piIp:$piPort")
                
                while (isRunning) {
                    val clientSocket = serverSocket?.accept() ?: break
                    Log.d(TAG, "MediaPlayer connected to proxy")
                    handleClient(clientSocket)
                }
            } catch (e: Exception) {
                Log.e(TAG, "Proxy error: ${e.message}")
            }
        }.start()
    }

    private fun handleClient(clientSocket: Socket) {
        Thread {
            var piSocket: Socket? = null
            try {
                Log.d(TAG, "Connecting to Pi at $piIp:$piPort")
                piSocket = Socket(piIp, piPort)
                
                val piIn = piSocket.getInputStream()
                val clientOut = clientSocket.getOutputStream()
                
                val buffer = ByteArray(4096 * 8) // 32KB buffer
                var bytesRead: Int
                
                while (isRunning && !clientSocket.isClosed && !piSocket.isClosed) {
                    bytesRead = piIn.read(buffer)
                    if (bytesRead == -1) break
                    clientOut.write(buffer, 0, bytesRead)
                }
            } catch (e: Exception) {
                Log.e(TAG, "Streaming error: ${e.message}")
            } finally {
                try { clientSocket.close() } catch (e: Exception) {}
                try { piSocket?.close() } catch (e: Exception) {}
                Log.d(TAG, "Client connection closed")
            }
        }.start()
    }

    fun stop() {
        isRunning = false
        try {
            serverSocket?.close()
        } catch (e: Exception) {}
    }
}
