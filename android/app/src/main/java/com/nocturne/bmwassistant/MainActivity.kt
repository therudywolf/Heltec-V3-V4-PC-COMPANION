package com.nocturne.bmwassistant

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
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
import java.util.UUID

class MainActivity : AppCompatActivity(), BleAssistantHost {

    private val controlServiceUuid = UUID.fromString("1a2b0001-5e6f-4a5b-8c9d-0e1f2a3b4c5d")
    private val controlCharUuid = UUID.fromString("1a2b0002-5e6f-4a5b-8c9d-0e1f2a3b4c5d")
    private val statusCharUuid = UUID.fromString("1a2b0003-5e6f-4a5b-8c9d-0e1f2a3b4c5d")
    private val nowPlayingCharUuid = UUID.fromString("1a2b0004-5e6f-4a5b-8c9d-0e1f2a3b4c5d")
    private val clusterTextCharUuid = UUID.fromString("1a2b0005-5e6f-4a5b-8c9d-0e1f2a3b4c5d")

    companion object {
        private const val LOG_TAG = "BMWAssistant"
        private const val deviceName = "BMW E39 Key"
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

    private var bluetoothAdapter: BluetoothAdapter? = null
    private var scanner: BluetoothLeScanner? = null
    private var gatt: BluetoothGatt? = null
    private var controlChar: BluetoothGattCharacteristic? = null
    private var statusChar: BluetoothGattCharacteristic? = null
    private var nowPlayingChar: BluetoothGattCharacteristic? = null
    private var clusterTextChar: BluetoothGattCharacteristic? = null

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

        onBackPressedDispatcher.addCallback(this, object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                if (pager.currentItem == 5) {
                    pager.setCurrentItem(4, true)
                } else {
                    finish()
                }
            }
        })

        bluetoothAdapter = BluetoothAdapter.getDefaultAdapter()
        if (bluetoothAdapter == null) {
            viewModel.setConnectionState("no_bt")
            return
        }

        requestPermissionsThen {
            buttonScan.setOnClickListener { startScan() }
            buttonDisconnect.setOnClickListener { disconnect() }
            if (getAutoConnectPref()) {
                getSharedPreferences(prefsName, Context.MODE_PRIVATE).getString(prefsLastDevice, null)?.let { addr ->
                    if (addr.isNotBlank()) tryAutoConnect(addr)
                }
            }
        }
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

    override fun isConnected(): Boolean = controlChar != null

    override fun sendCommand(byte: Int) {
        val c = controlChar ?: run {
            Toast.makeText(this, getString(R.string.hint_connect_first), Toast.LENGTH_SHORT).show()
            return
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) != android.content.pm.PackageManager.PERMISSION_GRANTED) return
        }
        c.value = byteArrayOf(byte.toByte())
        gatt?.writeCharacteristic(c)
    }

    override fun sendNowPlaying(track: String, artist: String) {
        val c = nowPlayingChar ?: return
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) != android.content.pm.PackageManager.PERMISSION_GRANTED) return
        }
        val payload = if (artist.isEmpty()) track.toByteArray(Charsets.UTF_8)
        else (track + "\u0000" + artist).toByteArray(Charsets.UTF_8)
        c.value = payload
        gatt?.writeCharacteristic(c)
    }

    override fun sendClusterText(text: String) {
        val c = clusterTextChar ?: return
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) != android.content.pm.PackageManager.PERMISSION_GRANTED) return
        }
        val raw = text.toByteArray(Charsets.UTF_8)
        c.value = if (raw.size > 20) raw.copyOf(20) else raw
        gatt?.writeCharacteristic(c)
    }

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
        ActivityCompat.requestPermissions(this, perms.toTypedArray(), 1)
        handler.postDelayed(block, 500)
    }

    private fun tryAutoConnect(address: String) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S &&
            ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) != android.content.pm.PackageManager.PERMISSION_GRANTED) return
        val device = try { bluetoothAdapter?.getRemoteDevice(address) } catch (e: Exception) { null } ?: return
        viewModel.setConnectionState("connecting")
        buttonScan.isEnabled = false
        connect(device)
    }

    private fun startScan() {
        if (BluetoothAdapter.getDefaultAdapter()?.isEnabled != true) {
            Toast.makeText(this, "Turn on Bluetooth", Toast.LENGTH_SHORT).show()
            return
        }
        scanner = bluetoothAdapter?.bluetoothLeScanner
        if (scanner == null) {
            Toast.makeText(this, "BLE not available", Toast.LENGTH_SHORT).show()
            return
        }
        viewModel.setConnectionState("connecting")
        viewModel.clearStatus()
        buttonScan.isEnabled = false
        val filter = ScanFilter.Builder().setDeviceName(deviceName).build()
        val scanSettings = ScanSettings.Builder().build()
        val scanCallback = object : ScanCallback() {
            override fun onScanResult(callbackType: Int, result: ScanResult?) {
                result ?: return
                val dev = result.device ?: return
                if (dev.name == deviceName) {
                    scanner?.stopScan(this)
                    handler.post { connect(dev) }
                }
            }
            override fun onScanFailed(errorCode: Int) {
                handler.post {
                    viewModel.setConnectionState("disconnected")
                    buttonScan.isEnabled = true
                    Toast.makeText(this@MainActivity, "Scan failed: $errorCode", Toast.LENGTH_SHORT).show()
                }
            }
        }
        scanner?.startScan(listOf(filter), scanSettings, scanCallback)
        handler.postDelayed({
            scanner?.stopScan(scanCallback)
            if (controlChar == null) {
                viewModel.setConnectionState("disconnected")
                buttonScan.isEnabled = true
            }
        }, 15000)
    }

    private fun connect(device: BluetoothDevice) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S &&
            ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) != android.content.pm.PackageManager.PERMISSION_GRANTED) return
        gatt = device.connectGatt(this, false, gattCallback)
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt?, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                gatt?.discoverServices()
            } else {
                handler.post {
                    controlChar = null
                    statusChar = null
                    nowPlayingChar = null
                    clusterTextChar = null
                    this@MainActivity.gatt = null
                    viewModel.setConnectionState("disconnected")
                    viewModel.clearStatus()
                    buttonScan.isEnabled = true
                    buttonDisconnect.isEnabled = false
                }
            }
        }
        override fun onServicesDiscovered(gatt: BluetoothGatt?, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS || gatt == null) return
            val service = gatt.getService(controlServiceUuid) ?: return
            controlChar = service.getCharacteristic(controlCharUuid)
            statusChar = service.getCharacteristic(statusCharUuid)
            nowPlayingChar = service.getCharacteristic(nowPlayingCharUuid)
            clusterTextChar = service.getCharacteristic(clusterTextCharUuid)
            gatt.device?.address?.let { addr ->
                getSharedPreferences(prefsName, Context.MODE_PRIVATE).edit().putString(prefsLastDevice, addr).apply()
            }
            statusChar?.let { c ->
                if ((c.properties and BluetoothGattCharacteristic.PROPERTY_NOTIFY) != 0) {
                    gatt.setCharacteristicNotification(c, true)
                    c.descriptors?.firstOrNull()?.let { d ->
                        d.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                        gatt.writeDescriptor(d)
                    }
                }
            }
            handler.post {
                viewModel.setConnectionState("connected")
                buttonScan.isEnabled = false
                buttonDisconnect.isEnabled = true
            }
        }
        override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray) {
            if (characteristic.uuid == statusCharUuid && value.size >= 10) {
                handler.post { viewModel.setStatusFromPacket(value) }
            }
        }
    }

    private fun disconnect() {
        gatt?.disconnect()
        gatt?.close()
        gatt = null
        controlChar = null
        statusChar = null
        nowPlayingChar = null
        clusterTextChar = null
        viewModel.setConnectionState("disconnected")
        viewModel.clearStatus()
        buttonScan.isEnabled = true
        buttonDisconnect.isEnabled = false
    }

    override fun onDestroy() {
        disconnect()
        super.onDestroy()
    }
}
