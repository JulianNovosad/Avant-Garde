import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.isActive
import kotlinx.coroutines.async
import kotlinx.coroutines.coroutineScope

object PiDiscovery {
    private const val TAG = "PiDiscovery"
    private const val TARGET_PORT = 6005 // Use control listener port
    private const val CONNECT_TIMEOUT_MS = 150 // Slightly more aggressive
    private const val BATCH_SIZE = 64 // Faster scanning
    private const val SCAN_INTERVAL_MS = 5000L // Scan every 5 seconds

    private var monitoringJob: Job? = null
    private val _piIpFlow = MutableStateFlow<String?>(null)
    val piIpFlow: StateFlow<String?> = _piIpFlow

    fun startMonitoringPi(context: Context, scope: CoroutineScope) {
        if (monitoringJob?.isActive == true) return // Already monitoring

        monitoringJob = scope.launch(Dispatchers.IO) {
            var lastKnownPiIp: String? = null
            while (isActive) {
                val currentPiIp = findPiInternal(context)
                if (currentPiIp != lastKnownPiIp) {
                    Log.d(TAG, "Pi IP changed from $lastKnownPiIp to $currentPiIp")
                    _piIpFlow.value = currentPiIp
                    lastKnownPiIp = currentPiIp
                } else {
                    Log.d(TAG, "Pi IP remains $currentPiIp")
                }
                delay(SCAN_INTERVAL_MS)
            }
        }
        Log.i(TAG, "Started monitoring for Pi IP changes.")
    }

    fun stopMonitoringPi() {
        monitoringJob?.cancel()
        monitoringJob = null
        Log.i(TAG, "Stopped monitoring for Pi IP changes.")
    }

    private suspend fun findPiInternal(context: Context): String? {
        return withContext(Dispatchers.IO) {
        val connectivityManager = context.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
        val candidates = mutableListOf<Pair<String, Boolean>>()

        // 1. ConnectivityManager Scan
        val networks = connectivityManager.allNetworks
        Log.d(TAG, "Scanning ${networks.size} networks via ConnectivityManager")
        for (network in networks) {
            val lp = connectivityManager.getLinkProperties(network) ?: continue
            val capabilities = connectivityManager.getNetworkCapabilities(network)
            val isEthernet = capabilities?.hasTransport(android.net.NetworkCapabilities.TRANSPORT_ETHERNET) == true ||
                             capabilities?.hasTransport(android.net.NetworkCapabilities.TRANSPORT_USB) == true
            
            Log.d(TAG, "Processing network: ${lp.interfaceName}, isEthernet/USB: $isEthernet")

            for (linkAddress in lp.linkAddresses) {
                val address = linkAddress.address
                if (address is Inet4Address && !address.isLoopbackAddress) {
                    Log.d(TAG, "Found local IP on interface ${lp.interfaceName}: ${address.hostAddress}/${linkAddress.prefixLength}")
                    val piIp = scanSubnet(address, linkAddress.prefixLength)
                    if (piIp != null) candidates.add(piIp to isEthernet)
                }
            }
        }

        // 2. Legacy Interface Scan (Catch-all for tethered links)
        Log.d(TAG, "Running legacy probe for all network interfaces...")
        try {
            val interfaces = NetworkInterface.getNetworkInterfaces()
            while (interfaces.hasMoreElements()) {
                val iface = interfaces.nextElement()
                if (iface.isLoopback || !iface.isUp) {
                    Log.d(TAG, "Skipping interface ${iface.name}: isLoopback=${iface.isLoopback}, isUp=${iface.isUp}")
                    continue
                }
                
                val isEthernet = iface.name.contains("eth") || iface.name.contains("usb") || iface.name.contains("rndis")
                Log.d(TAG, "Processing interface: ${iface.name}, isEthernet/USB: $isEthernet")

                for (ia in iface.interfaceAddresses) {
                    val addr = ia.address
                    if (addr is Inet4Address && !addr.isLoopbackAddress) {
                        Log.d(TAG, "Found local IP on interface ${iface.name}: ${addr.hostAddress}/${ia.networkPrefixLength}")
                        val piIp = scanSubnet(addr, ia.networkPrefixLength.toInt())
                        if (piIp != null) candidates.add(piIp to isEthernet)
                    } else {
                        Log.d(TAG, "Skipping non-IPv4 or loopback address: ${addr.hostAddress}")
                    }
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Legacy probe error: ${e.message}")
        }

        // 3. Evaluation: Prefer Ethernet, then any found Pi
        val best = candidates.find { it.second } ?: candidates.firstOrNull()
        
        if (best != null) {
            Log.i(TAG, "FINAL PI SELECTION: ${best.first} (Ethernet priority: ${best.second})")
            return@withContext best.first
        }

        null
        }
    }

    private suspend fun scanSubnet(localIp: Inet4Address, prefixLength: Int): String? = coroutineScope {
        if (prefixLength < 16) {
            Log.w(TAG, "Subnet too large for fast scan (/${prefixLength}) for local IP ${localIp.hostAddress}. Skipping.")
            return@coroutineScope null
        }

        val ipBytes = localIp.address
        val ipInt = ((ipBytes[0].toInt() and 0xFF) shl 24) or
                    ((ipBytes[1].toInt() and 0xFF) shl 16) or
                    ((ipBytes[2].toInt() and 0xFF) shl 8) or
                    (ipBytes[3].toInt() and 0xFF)
        
        val mask = if (prefixLength == 0) 0 else -1 shl (32 - prefixLength)
        val network = ipInt and mask
        val broadcast = ipInt or mask.inv()
        
        val startIp = network + 1
        val endIp = broadcast - 1
        val myIpStr = localIp.hostAddress

        Log.d(TAG, "Scanning subnet ${intToIp(network)}/${prefixLength} (${intToIp(startIp)}-${intToIp(endIp)}) from local IP ${myIpStr}")

        val ipsToScan = (startIp..endIp).toList()
        var checkedCount = 0
        for (batch in ipsToScan.chunked(BATCH_SIZE)) {
            val deferreds = batch.map { currentIpInt ->
                async {
                    val ipStr = intToIp(currentIpInt)
                    if (ipStr == myIpStr) return@async null
                    
                    val portCheckResult = checkPort(ipStr)
                    Log.d(TAG, "Checked IP $ipStr: ${if (portCheckResult) "FOUND" else "NOT FOUND"}") // Detailed log
                    if (portCheckResult) return@async ipStr
                    null
                }
            }
            
            for (d in deferreds) {
                val result = d.await()
                if (result != null) {
                    Log.i(TAG, "Pi found at ${result} on port $TARGET_PORT!")
                    return@coroutineScope result
                }
            }
            checkedCount += batch.size
            Log.d(TAG, "Scanned $checkedCount IPs in subnet ${intToIp(network)}/${prefixLength}...")
        }
        Log.d(TAG, "Finished scanning subnet ${intToIp(network)}/${prefixLength}. No Pi found.")
        null
    }

    private suspend fun legacyProbe(): String? {
        try {
            val interfaces = NetworkInterface.getNetworkInterfaces()
            while (interfaces.hasMoreElements()) {
                val iface = interfaces.nextElement()
                if (iface.isLoopback || !iface.isUp) continue
                
                // For legacy purposes, we scan addresses on UP interfaces
                val addresses = iface.inetAddresses
                while (addresses.hasMoreElements()) {
                    val addr = addresses.nextElement()
                    if (addr is Inet4Address && !addr.isLoopbackAddress) {
                        // We don't have prefix here easily in all API levels via this call,
                        // but InterfaceAddress provides it.
                        val interfaceAddresses = iface.interfaceAddresses
                        for (ia in interfaceAddresses) {
                            val iaAddr = ia.address
                            if (iaAddr is Inet4Address && !iaAddr.isLoopbackAddress) {
                                val piIp = scanSubnet(iaAddr, ia.networkPrefixLength.toInt())
                                if (piIp != null) return piIp
                            }
                        }
                    }
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Legacy probe error: ${e.message}")
        }
        return null
    }

    private fun intToIp(ipInt: Int): String {
        return String.format("%d.%d.%d.%d",
            (ipInt shr 24) and 0xFF,
            (ipInt shr 16) and 0xFF,
            (ipInt shr 8) and 0xFF,
            ipInt and 0xFF)
    }

    private fun checkPort(ip: String): Boolean {
        return try {
            val socket = Socket()
            socket.setTcpNoDelay(true)
            socket.connect(InetSocketAddress(ip, TARGET_PORT), CONNECT_TIMEOUT_MS)
            socket.close()
            Log.d(TAG, "PI FOUND AT $ip")
            true
        } catch (e: Exception) {
            false
        }
    }
}
