package com.nocturne.bmwassistant

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch

class BusFragment : Fragment() {
    private val viewModel: BleAssistantViewModel by activityViewModels()

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        return inflater.inflate(R.layout.fragment_bus, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        val connectionState = view.findViewById<TextView>(R.id.busConnectionState)
        val flags = view.findViewById<TextView>(R.id.busFlags)
        val temperatures = view.findViewById<TextView>(R.id.busTemperatures)
        val rpm = view.findViewById<TextView>(R.id.busRpm)
        val pdc = view.findViewById<TextView>(R.id.busPdc)
        val mfl = view.findViewById<TextView>(R.id.busMfl)
        val doorsLockIgn = view.findViewById<TextView>(R.id.busDoorsLockIgn)
        val odometer = view.findViewById<TextView>(R.id.busOdometer)
        val rawHex = view.findViewById<TextView>(R.id.busRawHex)

        viewLifecycleOwner.lifecycleScope.launch {
            viewModel.connectionState.collectLatest { state ->
                connectionState.text = when (state) {
                    "connected" -> getString(R.string.status_connected)
                    "connecting" -> getString(R.string.status_connecting)
                    "no_bt" -> "No Bluetooth"
                    else -> getString(R.string.status_disconnected)
                }
            }
        }

        viewLifecycleOwner.lifecycleScope.launch {
            combine(
                viewModel.statusData,
                viewModel.rawStatusHex
            ) { data, raw ->
                Pair(data, raw)
            }.collectLatest { (data, raw) ->
                if (data == null) {
                    flags.text = "—"
                    temperatures.text = "—"
                    rpm.text = "RPM: —"
                    pdc.text = "FL —  FR —  RL —  RR —"
                    mfl.text = "—"
                    doorsLockIgn.text = "—"
                    odometer.text = "${getString(R.string.status_odometer_km)}: —"
                    rawHex.text = raw ?: "—"
                    return@collectLatest
                }
                flags.text = buildString {
                    append(getString(R.string.bus_ibus_synced)); append(if (data.ibusOk) " ✓" else " —")
                    append("\n"); append(getString(R.string.bus_phone_connected)); append(if (data.phoneConnected) " ✓" else " —")
                    append("\n"); append(getString(R.string.bus_pdc_valid)); append(if (data.pdcValid) " ✓" else " —")
                    append("\n"); append(getString(R.string.bus_obd_connected)); append(if (data.obdOk) " ✓" else " —")
                }
                val coolantStr = data.coolantC?.let { "$it °C" } ?: "—"
                val oilStr = data.oilC?.let { "$it °C" } ?: "—"
                temperatures.text = "${getString(R.string.coolant)}: $coolantStr  ·  ${getString(R.string.oil)}: $oilStr"
                rpm.text = "${getString(R.string.rpm)}: ${data.rpm?.toString() ?: "—"}"
                pdc.text = "FL ${data.pdcFl ?: "—"}  FR ${data.pdcFr ?: "—"}  RL ${data.pdcRl ?: "—"}  RR ${data.pdcRr ?: "—"} cm"
                mfl.text = data.mflAction
                val doorsStr = when {
                    data.doorByte1 != null && data.doorByte2 != null ->
                        String.format("%s 0x%02X 0x%02X", getString(R.string.status_doors), data.doorByte1, data.doorByte2)
                    else -> "${getString(R.string.status_doors)}: —"
                }
                val lockStr = data.lockState?.let {
                    when (it) {
                        0 -> getString(R.string.lock_unlocked)
                        1 -> getString(R.string.lock_locked)
                        2 -> getString(R.string.lock_double)
                        else -> "—"
                    }
                } ?: "—"
                val ignStr = data.ignition?.let {
                    when (it) {
                        0 -> getString(R.string.ign_off)
                        1 -> getString(R.string.ign_pos1)
                        2 -> getString(R.string.ign_pos2)
                        else -> "—"
                    }
                } ?: "—"
                doorsLockIgn.text = "$doorsStr\n${getString(R.string.status_lock)}: $lockStr  ·  ${getString(R.string.status_ignition)}: $ignStr"
                odometer.text = "${getString(R.string.status_odometer_km)}: ${data.odometerKm?.toString() ?: "—"}"
                rawHex.text = raw ?: "—"
            }
        }
    }
}
