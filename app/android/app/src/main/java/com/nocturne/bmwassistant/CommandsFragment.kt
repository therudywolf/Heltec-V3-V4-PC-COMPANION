package com.nocturne.bmwassistant

import android.app.AlertDialog
import android.os.Handler
import android.os.Looper
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import com.google.android.material.chip.Chip

class CommandsFragment : Fragment() {
    private val handler = Handler(Looper.getMainLooper())

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: android.os.Bundle?): View {
        return inflater.inflate(R.layout.fragment_commands, container, false)
    }

    private fun sendWithConfirmIfNeeded(main: MainActivity, run: () -> Unit) {
        if (main.getConfirmDangerousPref()) {
            AlertDialog.Builder(requireContext())
                .setTitle(getString(R.string.confirm_dangerous_title))
                .setMessage(getString(R.string.confirm_dangerous_message))
                .setPositiveButton(getString(R.string.confirm)) { _, _ -> run() }
                .setNegativeButton(getString(R.string.cancel), null)
                .show()
        } else run()
    }

    override fun onViewCreated(view: View, savedInstanceState: android.os.Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        val host = activity as? BleAssistantHost ?: return
        val main = activity as? MainActivity
        val clusterBtn = view.findViewById<com.google.android.material.button.MaterialButton>(R.id.btnCluster)
        clusterBtn.text = if (host.isE46Model()) getString(R.string.cluster_kombi) else getString(R.string.cluster_ike)

        view.findViewById<View>(R.id.btnScenarioLeave).setOnClickListener {
            if (main == null) {
                host.sendCommand(0x07)
                handler.postDelayed({ host.sendCommand(0x05) }, 250)
                return@setOnClickListener
            }
            sendWithConfirmIfNeeded(main) {
                val delay = 350L
                var at = 0L
                if (main.getScenarioLeaveCloseWindows()) {
                    listOf(13, 15, 17, 19).forEach { byte ->
                        handler.postDelayed({ host.sendCommand(byte) }, at)
                        at += delay
                    }
                }
                if (main.getScenarioLeaveLock()) {
                    handler.postDelayed({ host.sendCommand(0x07) }, at)
                    at += delay
                }
                if (main.getScenarioLeaveFollowMeHome()) {
                    handler.postDelayed({ host.sendCommand(0x01) }, at)
                    at += delay
                }
                if (main.getScenarioLeaveGoodbyeLights()) {
                    handler.postDelayed({ host.sendCommand(0x00) }, at)
                }
            }
        }
        view.findViewById<View>(R.id.btnScenarioReturned).setOnClickListener { host.sendCommand(0x06) }
        view.findViewById<View>(R.id.btnScenarioNight).setOnClickListener {
            host.sendCommand(0x05)
            handler.postDelayed({ host.sendCommand(0x07) }, 250)
        }

        view.findViewById<Chip>(R.id.btnGoodbye).setOnClickListener { host.sendCommand(0x00) }
        view.findViewById<Chip>(R.id.btnFollowMe).setOnClickListener { host.sendCommand(0x01) }
        view.findViewById<Chip>(R.id.btnPark).setOnClickListener { host.sendCommand(0x02) }
        view.findViewById<Chip>(R.id.btnHazard).setOnClickListener { host.sendCommand(0x03) }
        view.findViewById<Chip>(R.id.btnLowBeams).setOnClickListener { host.sendCommand(0x04) }
        view.findViewById<Chip>(R.id.btnLightsOff).setOnClickListener {
            if (main != null) sendWithConfirmIfNeeded(main) { host.sendCommand(0x05) }
            else host.sendCommand(0x05)
        }
        view.findViewById<View>(R.id.btnUnlock).setOnClickListener { host.sendCommand(0x06) }
        view.findViewById<View>(R.id.btnLock).setOnClickListener {
            if (main != null) sendWithConfirmIfNeeded(main) { host.sendCommand(0x07) }
            else host.sendCommand(0x07)
        }
        view.findViewById<View>(R.id.btnTrunk).setOnClickListener { host.sendCommand(0x08) }
        view.findViewById<View>(R.id.btnCluster).setOnClickListener { host.sendCommand(0x09) }
        view.findViewById<View>(R.id.btnDoorsUnlock).setOnClickListener { host.sendCommand(0x0A) }
        view.findViewById<View>(R.id.btnDoorsLock).setOnClickListener { host.sendCommand(0x0B) }

        view.findViewById<com.google.android.material.chip.Chip>(R.id.btnHardLock).setOnClickListener {
            if (main != null) sendWithConfirmIfNeeded(main) { host.sendCommand(25) }
            else host.sendCommand(25)
        }
        view.findViewById<com.google.android.material.chip.Chip>(R.id.btnAllExceptDriver).setOnClickListener {
            if (main != null) sendWithConfirmIfNeeded(main) { host.sendCommand(26) }
            else host.sendCommand(26)
        }
        view.findViewById<com.google.android.material.chip.Chip>(R.id.btnDriverDoorOnly).setOnClickListener {
            if (main != null) sendWithConfirmIfNeeded(main) { host.sendCommand(27) }
            else host.sendCommand(27)
        }
        view.findViewById<com.google.android.material.chip.Chip>(R.id.btnDoorsFuelTrunk).setOnClickListener {
            if (main != null) sendWithConfirmIfNeeded(main) { host.sendCommand(28) }
            else host.sendCommand(28)
        }

        view.findViewById<Chip>(R.id.btnWinFrontLOpen).setOnClickListener { host.sendCommand(12) }
        view.findViewById<Chip>(R.id.btnWinFrontLClose).setOnClickListener { host.sendCommand(13) }
        view.findViewById<Chip>(R.id.btnWinFrontROpen).setOnClickListener { host.sendCommand(14) }
        view.findViewById<Chip>(R.id.btnWinFrontRClose).setOnClickListener { host.sendCommand(15) }
        view.findViewById<Chip>(R.id.btnWinRearLOpen).setOnClickListener { host.sendCommand(16) }
        view.findViewById<Chip>(R.id.btnWinRearLClose).setOnClickListener { host.sendCommand(17) }
        view.findViewById<Chip>(R.id.btnWinRearROpen).setOnClickListener { host.sendCommand(18) }
        view.findViewById<Chip>(R.id.btnWinRearRClose).setOnClickListener { host.sendCommand(19) }
        view.findViewById<View>(R.id.btnWipers).setOnClickListener { host.sendCommand(20) }
        view.findViewById<View>(R.id.btnWasher).setOnClickListener { host.sendCommand(21) }
        view.findViewById<View>(R.id.btnInteriorOff).setOnClickListener { host.sendCommand(22) }
        view.findViewById<View>(R.id.btnInteriorOn).setOnClickListener { host.sendCommand(23) }
        view.findViewById<View>(R.id.btnClown).setOnClickListener { host.sendCommand(24) }

        view.findViewById<View>(R.id.btnLightShowOn).setOnClickListener { host.sendCommand(0x80) }
        view.findViewById<View>(R.id.btnLightShowOff).setOnClickListener { host.sendCommand(0x81) }
    }
}
