package com.wiredvideoviewer

import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import java.io.InputStream
import java.io.OutputStream
import java.net.InetAddress
import java.net.ServerSocket
import java.net.Socket
import java.net.SocketException

class VideoProxy(
    private val piIp: String,
    private val piPort: Int = 5000,
    private val scope: CoroutineScope // Pass CoroutineScope
) {
    private val TAG = "VideoProxy"
    private var serverSocket: ServerSocket? = null
    var proxyPort: Int = 0
        private set

    private var proxyJob: Job? = null // To manage the main proxy coroutine

    fun start() {
        if (proxyJob?.isActive == true) return // Already running
        
        proxyJob = scope.launch(Dispatchers.IO) {
            try {
                // Bind to localhost on random port
                serverSocket = ServerSocket(0, 0, InetAddress.getByName("127.0.0.1"))
                proxyPort = serverSocket?.localPort ?: 0
                Log.d(TAG, "Proxy started on 127.0.0.1:$proxyPort, targeting $piIp:$piPort")

                while (isActive) { // Use isActive for coroutine cancellation check
                    val clientSocket = try {
                        serverSocket?.accept()
                    } catch (e: SocketException) {
                        // This exception is expected when serverSocket.close() is called
                        Log.d(TAG, "ServerSocket accept interrupted, proxy stopping.")
                        break // Exit loop on expected interruption
                    } catch (e: Exception) {
                        Log.e(TAG, "Proxy accept error: ${e.message}")
                        break // Exit loop on other errors
                    } ?: break // Break if serverSocket is null

                    Log.d(TAG, "MediaPlayer connected to proxy")
                    handleClient(clientSocket)
                }
            } catch (e: Exception) {
                Log.e(TAG, "Proxy error: ${e.message}")
            } finally {
                serverSocket?.close()
                serverSocket = null
                Log.i(TAG, "VideoProxy serverSocket closed.")
            }
        }
    }

    private fun handleClient(clientSocket: Socket) = scope.launch(Dispatchers.IO) {
        var piSocket: Socket? = null
        try {
            Log.d(TAG, "Connecting to Pi at $piIp:$piPort")
            piSocket = Socket(piIp, piPort)
            
            val piIn = piSocket.getInputStream()
            val clientOut = clientSocket.getOutputStream()
            
            val buffer = ByteArray(4096 * 8) // 32KB buffer
            var bytesRead: Int
            
            while (isActive && !clientSocket.isClosed && !piSocket.isClosed) {
                // withContext ensures the blocking read is cancellable within the coroutine
                bytesRead = withContext(Dispatchers.IO) {
                    piIn.read(buffer)
                }
                if (bytesRead == -1) break
                clientOut.write(buffer, 0, bytesRead)
            }
        } catch (e: Exception) {
            Log.e(TAG, "Streaming error: ${e.message}")
        } finally {
            try { clientSocket.close() } catch (e: Exception) { Log.e(TAG, "Error closing clientSocket: ${e.message}")}
            try { piSocket?.close() } catch (e: Exception) { Log.e(TAG, "Error closing piSocket: ${e.message}")}
            Log.d(TAG, "Client connection closed")
        }
    }

    fun stop() {
        proxyJob?.cancel() // Cancel the main proxy coroutine
        proxyJob = null
        try {
            serverSocket?.close() // Close the server socket to unblock accept()
        } catch (e: Exception) {
            Log.e(TAG, "Error closing server socket during stop: ${e.message}")
        }
        Log.i(TAG, "VideoProxy stopped.")
    }
}
