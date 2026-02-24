package com.nocturne.bmwassistant

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.lifecycleScope
import com.google.android.material.switchmaterial.SwitchMaterial
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch

class DashboardFragment : Fragment() {
    private val viewModel: BleAssistantViewModel by activityViewModels()

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        return inflater.inflate(R.layout.fragment_dashboard, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        val connectionState = view.findViewById<TextView>(R.id.dashboardConnectionState)
        val busSensors = view.findViewById<TextView>(R.id.dashboardBusSensors)
        val vehicle = view.findViewById<TextView>(R.id.dashboardVehicle)
        val pdcMfl = view.findViewById<TextView>(R.id.dashboardPdcMfl)
        val autoConnect = view.findViewById<SwitchMaterial>(R.id.dashboardAutoConnect)

        (activity as? MainActivity)?.let { act ->
            autoConnect.isChecked = act.getAutoConnectPref()
            autoConnect.setOnCheckedChangeListener { _, isChecked -> act.setAutoConnectPref(isChecked) }
        }

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
            viewModel.statusData.collectLatest { data ->
                if (data == null) {
                    busSensors.text = "—"
                    vehicle.text = "—"
                    pdcMfl.text = "—"
                    return@collectLatest
                }
                val coolantStr = data.coolantC?.let { "$it °C" } ?: "—"
                val oilStr = data.oilC?.let { "$it °C" } ?: "—"
                val rpmStr = data.rpm?.toString() ?: "—"
                busSensors.text = """
                    ${getString(R.string.ibus)}: ${if (data.ibusOk) "OK" else "—"} · ${getString(R.string.obd)}: ${if (data.obdOk) "OK" else "—"}
                    ${getString(R.string.coolant)}: $coolantStr · ${getString(R.string.oil)}: $oilStr · ${getString(R.string.rpm)}: $rpmStr
                """.trimIndent()

                val doorsStr = when {
                    data.doorByte1 != null && data.doorByte2 != null ->
                        String.format("0x%02X 0x%02X", data.doorByte1, data.doorByte2)
                    else -> "—"
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
                val odomStr = data.odometerKm?.toString() ?: "—"
                vehicle.text = """
                    ${getString(R.string.status_doors)}: $doorsStr · ${getString(R.string.status_lock)}: $lockStr
                    ${getString(R.string.status_ignition)}: $ignStr · ${getString(R.string.status_odometer_km)}: $odomStr
                """.trimIndent()

                val pdcStr = if (data.pdcFl != null || data.pdcFr != null)
                    "FL ${data.pdcFl ?: "—"} FR ${data.pdcFr ?: "—"} RL ${data.pdcRl ?: "—"} RR ${data.pdcRr ?: "—"} cm"
                else "—"
                pdcMfl.text = """
                    ${getString(R.string.pdc)}: $pdcStr
                    ${getString(R.string.mfl)}: ${data.mflAction}
                """.trimIndent()
            }
        }
    }
}
