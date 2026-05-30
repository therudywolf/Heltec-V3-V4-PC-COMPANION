package com.nocturne.bmwassistant

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.lifecycleScope
import com.google.android.material.materialswitch.MaterialSwitch
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
        val autoConnect = view.findViewById<MaterialSwitch>(R.id.dashboardAutoConnect)
        val lockTv = view.findViewById<TextView>(R.id.dashboardLock)
        val doorsTv = view.findViewById<TextView>(R.id.dashboardDoors)
        val ignitionTv = view.findViewById<TextView>(R.id.dashboardIgnition)
        val odometerTv = view.findViewById<TextView>(R.id.dashboardOdometer)
        val coolantTv = view.findViewById<TextView>(R.id.dashboardCoolant)
        val oilTv = view.findViewById<TextView>(R.id.dashboardOil)
        val rpmTv = view.findViewById<TextView>(R.id.dashboardRpm)
        val pdcFlTv = view.findViewById<TextView>(R.id.dashboardPdcFl)
        val pdcFrTv = view.findViewById<TextView>(R.id.dashboardPdcFr)
        val pdcRlTv = view.findViewById<TextView>(R.id.dashboardPdcRl)
        val pdcRrTv = view.findViewById<TextView>(R.id.dashboardPdcRr)
        val mflTv = view.findViewById<TextView>(R.id.dashboardMfl)

        (activity as? MainActivity)?.let { act ->
            autoConnect.isChecked = act.getAutoConnectPref()
            autoConnect.setOnCheckedChangeListener { _, isChecked -> act.setAutoConnectPref(isChecked) }
        }

        fun setPlaceholders() {
            lockTv.text = "—"
            doorsTv.text = "—"
            ignitionTv.text = "—"
            odometerTv.text = "—"
            coolantTv.text = "—"
            oilTv.text = "—"
            rpmTv.text = "—"
            pdcFlTv.text = "—"
            pdcFrTv.text = "—"
            pdcRlTv.text = "—"
            pdcRrTv.text = "—"
            mflTv.text = "—"
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
                    setPlaceholders()
                    return@collectLatest
                }
                lockTv.text = data.lockState?.let {
                    when (it) {
                        0 -> getString(R.string.lock_unlocked)
                        1 -> getString(R.string.lock_locked)
                        2 -> getString(R.string.lock_double)
                        else -> "—"
                    }
                } ?: "—"
                doorsTv.text = when {
                    data.doorByte1 != null && data.doorByte2 != null ->
                        String.format("0x%02X 0x%02X", data.doorByte1, data.doorByte2)
                    else -> "—"
                }
                ignitionTv.text = data.ignition?.let {
                    when (it) {
                        0 -> getString(R.string.ign_off)
                        1 -> getString(R.string.ign_pos1)
                        2 -> getString(R.string.ign_pos2)
                        else -> "—"
                    }
                } ?: "—"
                odometerTv.text = data.odometerKm?.let { "$it km" } ?: "—"

                coolantTv.text = data.coolantC?.let { "$it °C" } ?: "—"
                oilTv.text = data.oilC?.let { "$it °C" } ?: "—"
                rpmTv.text = data.rpm?.toString() ?: "—"

                pdcFlTv.text = data.pdcFl?.let { "$it cm" } ?: "—"
                pdcFrTv.text = data.pdcFr?.let { "$it cm" } ?: "—"
                pdcRlTv.text = data.pdcRl?.let { "$it cm" } ?: "—"
                pdcRrTv.text = data.pdcRr?.let { "$it cm" } ?: "—"

                mflTv.text = data.mflAction
            }
        }
    }
}
