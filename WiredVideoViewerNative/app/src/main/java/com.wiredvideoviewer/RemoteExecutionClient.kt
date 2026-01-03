package com.wiredvideoviewer

import android.util.Log
import com.jcraft.jsch.JSch
import com.jcraft.jsch.Session
import com.jcraft.jsch.ChannelExec
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import java.io.ByteArrayOutputStream
import java.io.OutputStream

object RemoteExecutionClient {
    private const val TAG = "RemoteExecutionClient"
    private const val PI_SSH_PORT = 22

    // Function to execute a command on the remote Pi via SSH
    fun executeCommand(piIp: String, command: String, log: (String) -> Unit) {
        CoroutineScope(Dispatchers.IO).launch {
            var session: Session? = null
            var channel: ChannelExec? = null
            log("Executing SSH command: '$command' on $piIp:$PI_SSH_PORT")
            try {
                val jsch = JSch()
                session = jsch.getSession("pi", piIp, PI_SSH_PORT)
                session.setPassword("pi")

                // Disable strict host key checking.
                // This is a security risk in a production environment,
                // but acceptable for a local network with a known device.
                session.setConfig("StrictHostKeyChecking", "no")

                session.connect(30000) // 30-second connection timeout
                log("SSH Session connected.")

                channel = session.openChannel("exec") as ChannelExec
                channel.setCommand(command)
                
                val commandOutput = ByteArrayOutputStream()
                val errorOutput = ByteArrayOutputStream()
                channel.setOutputStream(commandOutput)
                channel.setErrStream(errorOutput)

                // No longer piping stdin in this version of the function

                channel.connect(10000) // 10-second channel connection timeout

                // Wait for the remote command to finish
                while (channel.isConnected) {
                    Thread.sleep(100)
                }
                
                val exitStatus = channel.exitStatus
                val outputString = commandOutput.toString()
                val errorString = errorOutput.toString()

                if (exitStatus == 0) {
                    log("SSH command executed successfully.")
                    if (outputString.isNotEmpty()) {
                        log("SSH output: $outputString")
                    }
                } else {
                    log("SSH command failed with exit status $exitStatus.")
                    if (errorString.isNotEmpty()) {
                        log("SSH error: $errorString")
                    } else if (outputString.isNotEmpty()) {
                        log("SSH output (non-zero exit): $outputString")
                    }
                }
                Log.d(TAG, "SSH command exit status: $exitStatus")

            } catch (e: Exception) {
                val errorMessage = "SSH execution failed: ${e.message}"
                log(errorMessage)
                Log.e(TAG, errorMessage, e)
            } finally {
                channel?.disconnect()
                session?.disconnect()
                log("SSH Session and Channel disconnected.")
            }
        }
    }
}