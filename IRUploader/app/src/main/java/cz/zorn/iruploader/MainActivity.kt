package cz.zorn.iruploader

import android.hardware.ConsumerIrManager
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.layout.HorizontalRuler
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import cz.zorn.iruploader.ui.theme.IRUploaderTheme
import org.koin.androidx.viewmodel.ext.android.viewModel
import timber.log.Timber
import kotlin.getValue

class MainActivity : ComponentActivity() {
    private val model: MainActivityVM by viewModel()

    private lateinit var irManager: ConsumerIrManager

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        if (getSystemService(CONSUMER_IR_SERVICE) != null)
            irManager = getSystemService(CONSUMER_IR_SERVICE) as ConsumerIrManager

        enableEdgeToEdge()
        setContent {
            val serverState by model.serverState.collectAsState()

            IRUploaderTheme {
                Scaffold(modifier = Modifier.fillMaxSize()) { innerPadding ->
                    Column(modifier = Modifier.padding(innerPadding)) {
                        Sender(serverState)
                    }
                }
            }
        }
    }
}

@Composable
fun Sender(serverState: ServerState) {
    Column(Modifier.padding(15.dp)) {
        Text("IR SENDER", fontWeight = FontWeight.Bold)
        Text("Aplikace vytvori po spusteni server na portu 12345.")
        Text("Cokoliv poslete na tento port, bude odeslano pres IR vysilac protokolem pro bootloader")
        Text(
            "Muzete tak propgramovat sve ATTINY vyuzitim mobilniho telefonu",
            modifier = Modifier.padding(bottom = 10.dp)
        )

        when (serverState) {
            ServerState.Stopped -> Text("Server nebezi")
            is ServerState.Ready -> Text("Server bezi na adrese ${serverState.ip}")
        }
    }
}
