package com.nocturne.bmwassistant

import android.app.Application
import android.util.Log
import java.io.File
import java.io.PrintWriter
import java.io.StringWriter
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Logs uncaught exceptions to logcat (tag "BMWAssistant") and to a file
 * so you can capture the crash reason even when the app exits immediately.
 * File: getFilesDir()/crash_log.txt (append mode).
 */
class BmwAssistantApplication : Application() {

    override fun onCreate() {
        super.onCreate()
        val defaultHandler = Thread.getDefaultUncaughtExceptionHandler()
        Thread.setDefaultUncaughtExceptionHandler { thread, throwable ->
            val stack = StringWriter().use { sw ->
                PrintWriter(sw).use { pw -> throwable.printStackTrace(pw); sw.toString() }
            }
            val msg = "CRASH: ${throwable.message}\n$stack"
            Log.e(TAG, msg)
            try {
                val file = File(filesDir, "crash_log.txt")
                file.appendText(
                    "\n--- ${SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.US).format(Date())} ---\n$msg\n"
                )
            } catch (_: Exception) { }
            defaultHandler?.uncaughtException(thread, throwable)
        }
    }

    companion object {
        private const val TAG = "BMWAssistant"
    }
}
