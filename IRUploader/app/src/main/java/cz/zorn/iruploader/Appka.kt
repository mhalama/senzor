package cz.zorn.iruploader

import android.app.Application
import cz.zorn.iruploader.di.appModule
import kotlinx.coroutines.Job
import org.koin.android.ext.koin.androidContext
import org.koin.core.context.GlobalContext.startKoin
import org.koin.mp.KoinPlatform.getKoin
import timber.log.Timber

class Appka : Application() {
    override fun onCreate() {
        super.onCreate()

        startKoin {
            androidContext(this@Appka)
            modules(appModule)
        }

        getKoin().get<SocketServer>().start()

        Timber.plant(Timber.DebugTree())
    }

    override fun onTerminate() {
        super.onTerminate()

        getKoin().get<SocketServer>().stop()
        getKoin().get<Job>().cancel()
    }
}