package com.nocturne.bmwassistant

import androidx.lifecycle.ViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

data class BmwStatusData(
    val ibusOk: Boolean,
    val obdOk: Boolean,
    val coolantC: Int?,
    val oilC: Int?,
    val rpm: Int?,
    val pdcFl: Int?, val pdcFr: Int?, val pdcRl: Int?, val pdcRr: Int?,
    val mflAction: String,
    val doorByte1: Int?,
    val doorByte2: Int?,
    val lockState: Int?,
    val ignition: Int?,
    val odometerKm: Int?
)

class BleAssistantViewModel : ViewModel() {
    private val _connectionState = MutableStateFlow("disconnected")
    val connectionState: StateFlow<String> = _connectionState.asStateFlow()

    private val _statusData = MutableStateFlow<BmwStatusData?>(null)
    val statusData: StateFlow<BmwStatusData?> = _statusData.asStateFlow()

    fun setConnectionState(state: String) {
        _connectionState.value = state
    }

    fun setStatusFromPacket(value: ByteArray) {
        if (value.size < 10) return
        val flags = value[0].toInt() and 0xFF
        val coolant = value[1].toInt() and 0xFF
        val oil = value[2].toInt() and 0xFF
        val rpm = ((value[3].toInt() and 0xFF) shl 8) or (value[4].toInt() and 0xFF)
        val pdcFl = if (value[5].toInt() and 0xFF == 0xFF) null else value[5].toInt() and 0xFF
        val pdcFr = if (value[6].toInt() and 0xFF == 0xFF) null else value[6].toInt() and 0xFF
        val pdcRl = if (value[7].toInt() and 0xFF == 0xFF) null else value[7].toInt() and 0xFF
        val pdcRr = if (value[8].toInt() and 0xFF == 0xFF) null else value[8].toInt() and 0xFF
        val mfl = value[9].toInt() and 0xFF
        val mflStr = when (mfl) {
            1 -> "Next"
            2 -> "Prev"
            3 -> "Play/Pause"
            4 -> "Vol+"
            5 -> "Vol−"
            else -> "—"
        }
        var doorByte1: Int? = null
        var doorByte2: Int? = null
        var lockState: Int? = null
        var ignition: Int? = null
        var odometerKm: Int? = null
        if (value.size >= 16) {
            val d1 = value[10].toInt() and 0xFF
            val d2 = value[11].toInt() and 0xFF
            val lock = value[12].toInt() and 0xFF
            val ign = value[13].toInt() and 0xFF
            val odomLo = value[14].toInt() and 0xFF
            val odomHi = value[15].toInt() and 0xFF
            if (d1 != 0xFF || d2 != 0xFF) { doorByte1 = d1; doorByte2 = d2 }
            if (lock <= 2) lockState = lock
            if (ign <= 3) ignition = ign
            if (odomLo != 0xFF || odomHi != 0xFF) odometerKm = odomLo or (odomHi shl 8)
        }
        _statusData.value = BmwStatusData(
            ibusOk = (flags and 0x01) != 0,
            obdOk = (flags and 0x08) != 0,
            coolantC = if (coolant == 0xFF) null else coolant,
            oilC = if (oil == 0xFF) null else oil,
            rpm = if (rpm == 0xFFFF) null else rpm,
            pdcFl, pdcFr, pdcRl, pdcRr,
            mflAction = mflStr,
            doorByte1 = doorByte1,
            doorByte2 = doorByte2,
            lockState = lockState,
            ignition = ignition,
            odometerKm = odometerKm
        )
    }

    fun clearStatus() {
        _statusData.value = null
    }
}
