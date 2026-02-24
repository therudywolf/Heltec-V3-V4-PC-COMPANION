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
        val statusText = view.findViewById<TextView>(R.id.dashboardStatusText)
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
                    statusText.text = "—"
                    return@collectLatest
                }
                val coolantStr = data.coolantC?.let { "$it °C" } ?: "—"
                val oilStr = data.oilC?.let { "$it °C" } ?: "—"
                val rpmStr = data.rpm?.toString() ?: "—"
                val pdcStr = if (data.pdcFl != null || data.pdcFr != null)
                    "FL ${data.pdcFl ?: "—"} FR ${data.pdcFr ?: "—"} RL ${data.pdcRl ?: "—"} RR ${data.pdcRr ?: "—"} cm"
                else "—"
                statusText.text = """
                    ${getString(R.string.ibus)}: ${if (data.ibusOk) "OK" else "—"} | ${getString(R.string.obd)}: ${if (data.obdOk) "OK" else "—"}
                    ${getString(R.string.coolant)}: $coolantStr | ${getString(R.string.oil)}: $oilStr | ${getString(R.string.rpm)}: $rpmStr
                    ${getString(R.string.pdc)}: $pdcStr
                    ${getString(R.string.mfl)}: ${data.mflAction}
                """.trimIndent()
            }
        }
    }
}
