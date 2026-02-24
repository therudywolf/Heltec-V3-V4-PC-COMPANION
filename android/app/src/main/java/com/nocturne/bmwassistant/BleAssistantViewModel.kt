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
    val mflAction: String
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
        _statusData.value = BmwStatusData(
            ibusOk = (flags and 0x01) != 0,
            obdOk = (flags and 0x08) != 0,
            coolantC = if (coolant == 0xFF) null else coolant,
            oilC = if (oil == 0xFF) null else oil,
            rpm = if (rpm == 0xFFFF) null else rpm,
            pdcFl, pdcFr, pdcRl, pdcRr,
            mflAction = mflStr
        )
    }

    fun clearStatus() {
        _statusData.value = null
    }
}
