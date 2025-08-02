package cz.zorn.iruploader

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import timber.log.Timber
import java.io.InputStream
import java.net.Inet4Address
import java.net.NetworkInterface
import java.net.ServerSocket
import java.net.Socket

sealed class ServerState {
    data class Ready(val ip: String) : ServerState()
    object Stopped : ServerState()
}

interface SocketServer {
    fun start()
    fun stop()
    val state: StateFlow<ServerState>
}

class SocketServerImpl(
    val port: Int,
    private val scope: CoroutineScope,
    private val loader: HexLoader,
    private val messageSender: IRMessageSender,
    private val firmwareSender: IRFirmwareSender
) : SocketServer {
    private lateinit var serverSocket: ServerSocket

    private var running = false

    private val _state = MutableStateFlow<ServerState>(ServerState.Stopped)
    override val state: StateFlow<ServerState> = _state

    override fun start() {
        scope.launch(Dispatchers.IO) {
            try {
                serverSocket = ServerSocket(port)
                Timber.d("Server běží na portu $port")

                running = true
                _state.update { ServerState.Ready(getLocalIpAddress()) }

                while (running && !serverSocket.isClosed) {
                    val clientSocket: Socket = serverSocket.accept()
                    Timber.d("Připojen klient: ${clientSocket.inetAddress.hostAddress}")

                    sendFlashFromInputStream(clientSocket.getInputStream())

                    clientSocket.close()
                }
                running = false
                _state.update { ServerState.Stopped }
            } catch (e: Exception) {
                Timber.d("Chyba: ${e.message}")
            }
        }
    }

    override fun stop() {
        try {
            serverSocket.close()
        } catch (e: Exception) {
            Timber.w("Chyba při zavírání serveru: ${e.message}")
        }
        running = false
        _state.update { ServerState.Stopped }
        Timber.d("Server zastaven")
    }

    private fun getLocalIpAddress(): String {
        try {
            val interfaces = NetworkInterface.getNetworkInterfaces()
            for (intf in interfaces) {
                val addresses = intf.inetAddresses
                for (addr in addresses) {
                    if (!addr.isLoopbackAddress && addr is Inet4Address) {
                        return addr.hostAddress ?: continue
                    }
                }
            }
        } catch (e: Exception) {
            Timber.e("Chyba při získávání IP adresy: ${e.message}")
        }
        return "?"
    }

    private suspend fun sendFlashFromInputStream(inputStream: InputStream) {
        messageSender.sendMessage(MSG_START_BOOTLOADER)
        delay(DELAY_AFTER_BOOTLOADER_START)
        val hex = loader.loadFlashPages(inputStream, 512, 64)
        firmwareSender.sendFlash(hex)
    }

    companion object {
        const val DELAY_AFTER_BOOTLOADER_START = 6000.toLong()
        const val MSG_START_BOOTLOADER = "BOOT"
    }
}
