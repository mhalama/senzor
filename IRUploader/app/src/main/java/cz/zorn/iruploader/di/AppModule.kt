package cz.zorn.iruploader.di

import android.content.Context
import android.hardware.ConsumerIrManager
import cz.zorn.iruploader.HexLoader
import cz.zorn.iruploader.HexLoaderImpl
import cz.zorn.iruploader.IRFirmwareSender
import cz.zorn.iruploader.IRFirmwareSenderImpl
import cz.zorn.iruploader.IRMessageSender
import cz.zorn.iruploader.IRMessageSenderImpl
import cz.zorn.iruploader.IRTransmitter
import cz.zorn.iruploader.MainActivityVM
import cz.zorn.iruploader.SocketServer
import cz.zorn.iruploader.SocketServerImpl
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import org.koin.android.ext.koin.androidContext
import org.koin.core.module.dsl.bind
import org.koin.core.module.dsl.singleOf
import org.koin.core.module.dsl.viewModelOf
import org.koin.dsl.module
import timber.log.Timber

val appModule = module {
    single<Job> { SupervisorJob() }
    single<CoroutineScope> { CoroutineScope(Dispatchers.Default + get<Job>()) }
    single<SocketServer> {
        SocketServerImpl(
            port = 12345,
            scope = get(),
            loader = get(),
            firmwareSender = get(),
            messageSender = get()
        )
    }

    single<IRTransmitter> {
        IRTransmitter { freq, pattern ->
            val irManager = androidContext().getSystemService(Context.CONSUMER_IR_SERVICE) as ConsumerIrManager
            val pat = pattern.joinToString(" ") { it.toString() }
            Timber.d("IR (${pattern.size}): $pat")
            irManager.transmit(freq, pattern)
        }
    }

    singleOf(::HexLoaderImpl) { bind<HexLoader>() }
    singleOf(::IRFirmwareSenderImpl) { bind<IRFirmwareSender>() }
    singleOf(::IRMessageSenderImpl) { bind<IRMessageSender>() }

    viewModelOf(::MainActivityVM)
}