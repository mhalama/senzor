package cz.zorn.iruploader

import androidx.lifecycle.ViewModel

class MainActivityVM(
    private val socketServer: SocketServer
) : ViewModel() {
    val serverState = socketServer.state
}