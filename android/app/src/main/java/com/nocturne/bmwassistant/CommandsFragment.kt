package com.nocturne.bmwassistant

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import com.google.android.material.chip.Chip

class CommandsFragment : Fragment() {
    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: android.os.Bundle?): View {
        return inflater.inflate(R.layout.fragment_commands, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: android.os.Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        val host = activity as? BleAssistantHost ?: return
        view.findViewById<Chip>(R.id.btnGoodbye).setOnClickListener { host.sendCommand(0x00) }
        view.findViewById<Chip>(R.id.btnFollowMe).setOnClickListener { host.sendCommand(0x01) }
        view.findViewById<Chip>(R.id.btnPark).setOnClickListener { host.sendCommand(0x02) }
        view.findViewById<Chip>(R.id.btnHazard).setOnClickListener { host.sendCommand(0x03) }
        view.findViewById<Chip>(R.id.btnLowBeams).setOnClickListener { host.sendCommand(0x04) }
        view.findViewById<Chip>(R.id.btnLightsOff).setOnClickListener { host.sendCommand(0x05) }
        view.findViewById<View>(R.id.btnUnlock).setOnClickListener { host.sendCommand(0x06) }
        view.findViewById<View>(R.id.btnLock).setOnClickListener { host.sendCommand(0x07) }
        view.findViewById<View>(R.id.btnTrunk).setOnClickListener { host.sendCommand(0x08) }
        view.findViewById<View>(R.id.btnCluster).setOnClickListener { host.sendCommand(0x09) }
        view.findViewById<View>(R.id.btnDoorsUnlock).setOnClickListener { host.sendCommand(0x0A) }
        view.findViewById<View>(R.id.btnDoorsLock).setOnClickListener { host.sendCommand(0x0B) }
        view.findViewById<View>(R.id.btnLightShowOn).setOnClickListener { host.sendCommand(0x80) }
        view.findViewById<View>(R.id.btnLightShowOff).setOnClickListener { host.sendCommand(0x81) }
    }
}
