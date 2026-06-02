package com.nocturne.bmwassistant

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.content.Context
import android.content.res.Configuration
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.widget.Toast
import androidx.activity.OnBackPressedCallback
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.lifecycle.ViewModelProvider
import androidx.viewpager2.widget.ViewPager2
import com.google.android.material.button.MaterialButton
import com.google.android.material.tabs.TabLayout
import com.google.android.material.tabs.TabLayoutMediator

class MainActivity : AppCompatActivity(), BleAssistantHost {

    companion object {
        private const val LOG_TAG = "BMWAssistant"
        private const val REQUEST_PERMISSIONS = 100
        private const val prefsName = "bmw_assistant"
        private const val prefsLastDevice = "last_device_address"
        private const val prefsAutoConnect = "auto_connect"
        private const val prefsTheme = "theme"
        private const val prefsCarModel = "car_model"
        private const val prefsWelcomeCluster = "welcome_cluster"
        private const val prefsShiftRpm = "shift_rpm"
        private const val prefsShowTrackCluster = "show_track_cluster"
        private const val prefsConfirmDangerous = "confirm_dangerous"
        private const val prefsLeaveCloseWindows = "scenario_leave_close_windows"
        private const val prefsLeaveLock = "scenario_leave_lock"
        private const val prefsLeaveFollowMeHome = "scenario_leave_follow_me_home"
        private const val prefsLeaveGoodbyeLights = "scenario_leave_goodbye_lights"
    }

    private lateinit var ble: BmwBleManager

    private lateinit var viewModel: BleAssistantViewModel
    private lateinit var buttonScan: MaterialButton
    private lateinit var buttonDisconnect: MaterialButton
    private val handler = Handler(Looper.getMainLooper())

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        Log.d(LOG_TAG, "onCreate start")
        try {
            applyThemeFromPref()
        } catch (e: Exception) {
            Log.e(LOG_TAG, "applyThemeFromPref failed", e)
            setTheme(R.style.Theme_BMWAssistant)
        }
        Log.d(LOG_TAG, "setContentView start")
        try {
            setContentView(R.layout.activity_main)
        } catch (e: Exception) {
            Log.e(LOG_TAG, "setContentView failed", e)
            throw e
        }
        Log.d(LOG_TAG, "setContentView done")
        viewModel = ViewModelProvider(this)[BleAssistantViewModel::class.java]

        buttonScan = findViewById(R.id.buttonScan)
        buttonDisconnect = findViewById(R.id.buttonDisconnect)

        val pager = findViewById<ViewPager2>(R.id.pager)
        pager.adapter = BmwPagerAdapter(this)

        val tabs = findViewById<TabLayout>(R.id.tabs)
        TabLayoutMediator(tabs, pager) { tab, position ->
            tab.text = when (position) {
                0 -> getString(R.string.tab_dashboard)
                1 -> getString(R.string.tab_commands)
                2 -> getString(R.string.tab_media)
                3 -> getString(R.string.tab_cluster)
                4 -> getString(R.string.tab_settings)
                else -> getString(R.string.tab_bus)
            }
        }.attach()

        onBackPressedDispatcher.addCallback(this, object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                if (pager.currentItem == 5) {
                    pager.setCurrentItem(4, true)
                } else {
                    finish()
                }
            }
        })

        ble = BmwBleManager(this, handler)
        if (!ble.bluetoothAvailable) {
            viewModel.setConnectionState("no_bt")
            return
        }
        ble.listener = bleListener

        requestPermissionsThen {
            buttonScan.setOnClickListener { onScanClicked() }
            buttonDisconnect.setOnClickListener { ble.disconnect() }
            if (getAutoConnectPref()) {
                getSharedPreferences(prefsName, Context.MODE_PRIVATE).getString(prefsLastDevice, null)?.let { addr ->
                    if (addr.isNotBlank()) tryAutoConnect(addr)
                }
            }
        }
    }

    /** Bridges BmwBleManager events to the ViewModel + UI buttons. */
    private val bleListener = object : BmwBleManager.Listener {
        override fun onState(state: String) {
            viewModel.setConnectionState(state)
            when (state) {
                "connected" -> {
                    buttonScan.isEnabled = false
                    buttonDisconnect.isEnabled = true
                }
                "connecting" -> {
                    viewModel.clearStatus()
                    buttonScan.isEnabled = false
                }
                else -> { // disconnected
                    viewModel.clearStatus()
                    buttonScan.isEnabled = true
                    buttonDisconnect.isEnabled = false
                    if (getAutoConnectPref()) {
                        handler.postDelayed({
                            if (!ble.isConnected)
                                getSharedPreferences(prefsName, Context.MODE_PRIVATE)
                                    .getString(prefsLastDevice, null)?.let { addr ->
                                        if (addr.isNotBlank()) tryAutoConnect(addr)
                                    }
                        }, 2500)
                    }
                }
            }
        }
        override fun onStatusPacket(value: ByteArray) = viewModel.setStatusFromPacket(value)
        override fun onWriteFailed() {
            Toast.makeText(this@MainActivity, getString(R.string.toast_write_failed), Toast.LENGTH_SHORT).show()
        }
        override fun onDeviceAddress(address: String) {
            getSharedPreferences(prefsName, Context.MODE_PRIVATE).edit()
                .putString(prefsLastDevice, address).apply()
        }
    }

    private fun onScanClicked() {
        if (BluetoothAdapter.getDefaultAdapter()?.isEnabled != true) {
            Toast.makeText(this, "Turn on Bluetooth", Toast.LENGTH_SHORT).show()
            return
        }
        if (!ble.bluetoothAvailable) {
            Toast.makeText(this, "BLE not available", Toast.LENGTH_SHORT).show()
            return
        }
        ble.startScan()
    }

    /** Opens the I-Bus / Bus data screen (page 5). Called from Settings. */
    fun showBusScreen() {
        findViewById<ViewPager2>(R.id.pager).setCurrentItem(5, true)
    }

    fun getAutoConnectPref(): Boolean =
        getSharedPreferences(prefsName, Context.MODE_PRIVATE).getBoolean(prefsAutoConnect, false)

    fun setAutoConnectPref(value: Boolean) {
        getSharedPreferences(prefsName, Context.MODE_PRIVATE).edit().putBoolean(prefsAutoConnect, value).apply()
    }

    fun getThemePref(): String =
        getSharedPreferences(prefsName, Context.MODE_PRIVATE).getString(prefsTheme, "system") ?: "system"

    fun setThemePref(value: String) {
        getSharedPreferences(prefsName, Context.MODE_PRIVATE).edit().putString(prefsTheme, value).apply()
    }

    private fun applyThemeFromPref() {
        val theme = getThemePref()
        val isNight = (resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES
        when (theme) {
            "light" -> setTheme(R.style.Theme_BMWAssistant_Light)
            "dark" -> setTheme(R.style.Theme_BMWAssistant_Dark)
            else -> if (isNight) setTheme(R.style.Theme_BMWAssistant_Dark) else setTheme(R.style.Theme_BMWAssistant_Light)
        }
    }

    fun getCarModelPref(): String =
        getSharedPreferences(prefsName, Context.MODE_PRIVATE).getString(prefsCarModel, "e39_fl") ?: "e39_fl"

    fun setCarModelPref(value: String) {
        getSharedPreferences(prefsName, Context.MODE_PRIVATE).edit().putString(prefsCarModel, value).apply()
    }

    fun getWelcomeClusterPref(): String =
        getSharedPreferences(prefsName, Context.MODE_PRIVATE).getString(prefsWelcomeCluster, "") ?: ""

    fun setWelcomeClusterPref(value: String) {
        getSharedPreferences(prefsName, Context.MODE_PRIVATE).edit().putString(prefsWelcomeCluster, value.take(20)).apply()
    }

    fun getShiftRpmPref(): Int =
        getSharedPreferences(prefsName, Context.MODE_PRIVATE).getInt(prefsShiftRpm, 5500).coerceIn(1000, 8000)

    fun setShiftRpmPref(value: Int) {
        getSharedPreferences(prefsName, Context.MODE_PRIVATE).edit().putInt(prefsShiftRpm, value.coerceIn(1000, 8000)).apply()
    }

    fun getShowTrackClusterPref(): Boolean =
        getSharedPreferences(prefsName, Context.MODE_PRIVATE).getBoolean(prefsShowTrackCluster, true)

    fun setShowTrackClusterPref(value: Boolean) {
        getSharedPreferences(prefsName, Context.MODE_PRIVATE).edit().putBoolean(prefsShowTrackCluster, value).apply()
    }

    fun getConfirmDangerousPref(): Boolean =
        getSharedPreferences(prefsName, Context.MODE_PRIVATE).getBoolean(prefsConfirmDangerous, true)

    fun setConfirmDangerousPref(value: Boolean) {
        getSharedPreferences(prefsName, Context.MODE_PRIVATE).edit().putBoolean(prefsConfirmDangerous, value).apply()
    }

    fun getScenarioLeaveCloseWindows(): Boolean =
        getSharedPreferences(prefsName, Context.MODE_PRIVATE).getBoolean(prefsLeaveCloseWindows, true)

    fun setScenarioLeaveCloseWindows(value: Boolean) {
        getSharedPreferences(prefsName, Context.MODE_PRIVATE).edit().putBoolean(prefsLeaveCloseWindows, value).apply()
    }

    fun getScenarioLeaveLock(): Boolean =
        getSharedPreferences(prefsName, Context.MODE_PRIVATE).getBoolean(prefsLeaveLock, true)

    fun setScenarioLeaveLock(value: Boolean) {
        getSharedPreferences(prefsName, Context.MODE_PRIVATE).edit().putBoolean(prefsLeaveLock, value).apply()
    }

    fun getScenarioLeaveFollowMeHome(): Boolean =
        getSharedPreferences(prefsName, Context.MODE_PRIVATE).getBoolean(prefsLeaveFollowMeHome, false)

    fun setScenarioLeaveFollowMeHome(value: Boolean) {
        getSharedPreferences(prefsName, Context.MODE_PRIVATE).edit().putBoolean(prefsLeaveFollowMeHome, value).apply()
    }

    fun getScenarioLeaveGoodbyeLights(): Boolean =
        getSharedPreferences(prefsName, Context.MODE_PRIVATE).getBoolean(prefsLeaveGoodbyeLights, true)

    fun setScenarioLeaveGoodbyeLights(value: Boolean) {
        getSharedPreferences(prefsName, Context.MODE_PRIVATE).edit().putBoolean(prefsLeaveGoodbyeLights, value).apply()
    }

    /** E46 uses K-Bus; cluster label is KOMBI. Others use IKE. */
    override fun isE46Model(): Boolean {
        val m = getCarModelPref()
        return m == "e46" || m == "e46_fl"
    }

    override fun isConnected(): Boolean = ble.isConnected

    override fun sendCommand(byte: Int) {
        if (!ble.isConnected) {
            Toast.makeText(this, getString(R.string.hint_connect_first), Toast.LENGTH_SHORT).show()
            return
        }
        ble.sendCommand(byte)
    }

    override fun sendNowPlaying(track: String, artist: String) = ble.sendNowPlaying(track, artist)

    override fun sendClusterText(text: String) = ble.sendClusterText(text)

    private fun requestPermissionsThen(block: () -> Unit) {
        val perms = mutableListOf<String>()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            perms.add(Manifest.permission.BLUETOOTH_SCAN)
            perms.add(Manifest.permission.BLUETOOTH_CONNECT)
        } else {
            perms.add(Manifest.permission.ACCESS_FINE_LOCATION)
            perms.add(Manifest.permission.BLUETOOTH)
            perms.add(Manifest.permission.BLUETOOTH_ADMIN)
        }
        if (perms.all { ActivityCompat.checkSelfPermission(this, it) == android.content.pm.PackageManager.PERMISSION_GRANTED }) {
            block()
            return
        }
        ActivityCompat.requestPermissions(this, perms.toTypedArray(), REQUEST_PERMISSIONS)
        pendingPermissionsBlock = block
    }

    private var pendingPermissionsBlock: (() -> Unit)? = null

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode != REQUEST_PERMISSIONS) return
        pendingPermissionsBlock?.let { block ->
            pendingPermissionsBlock = null
            if (grantResults.isNotEmpty() && grantResults.all { it == android.content.pm.PackageManager.PERMISSION_GRANTED }) {
                block()
            } else {
                Toast.makeText(this, getString(R.string.permission_required_for_ble), Toast.LENGTH_LONG).show()
                buttonScan.isEnabled = false
            }
        }
    }

    private fun tryAutoConnect(address: String) {
        buttonScan.isEnabled = false
        ble.connectToAddress(address)
    }

    override fun onDestroy() {
        ble.disconnect()
        super.onDestroy()
    }
}
