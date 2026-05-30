package com.nocturne.bmwassistant

import android.content.Context
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.LinearLayout
import android.widget.Toast
import androidx.fragment.app.Fragment
import com.google.android.material.button.MaterialButton
import com.google.android.material.textfield.TextInputEditText
import org.json.JSONArray

class ClusterFragment : Fragment() {

    companion object {
        private const val PRESETS_KEY = "cluster_presets"
        private const val MAX_PRESETS = 10
    }

    private val prefs by lazy {
        requireContext().getSharedPreferences("cluster", Context.MODE_PRIVATE)
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        return inflater.inflate(R.layout.fragment_cluster, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        val host = activity as? BleAssistantHost ?: return
        view.findViewById<android.widget.TextView>(R.id.cluster_desc).text =
            getString(if (host.isE46Model()) R.string.cluster_desc_kombi else R.string.cluster_desc_ike)
        val editCluster = view.findViewById<TextInputEditText>(R.id.editClusterText)
        view.findViewById<MaterialButton>(R.id.buttonSendCluster).setOnClickListener {
            if (!host.isConnected()) {
                Toast.makeText(requireContext(), getString(R.string.hint_connect_first), Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            val text = editCluster.text?.toString()?.trim() ?: ""
            if (text.isEmpty()) {
                Toast.makeText(requireContext(), getString(R.string.cluster_hint), Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            host.sendClusterText(text)
            Toast.makeText(requireContext(), getString(R.string.toast_sent_to_cluster), Toast.LENGTH_SHORT).show()
        }

        val presetList = view.findViewById<LinearLayout>(R.id.presetList)
        fun loadPresets(): List<String> {
            val raw = prefs.getString(PRESETS_KEY, "[]") ?: "[]"
            return try {
                val arr = JSONArray(raw)
                (0 until arr.length()).map { arr.getString(it) }
            } catch (_: Exception) {
                emptyList()
            }
        }
        fun savePresets(list: List<String>) {
            prefs.edit().putString(PRESETS_KEY, JSONArray(list).toString()).apply()
        }
        fun refreshPresetButtons() {
            presetList.removeAllViews()
            for (preset in loadPresets()) {
                val btn = MaterialButton(requireContext(), null, com.google.android.material.R.attr.materialButtonOutlinedStyle).apply {
                    text = preset
                    layoutParams = LinearLayout.LayoutParams(
                        LinearLayout.LayoutParams.MATCH_PARENT,
                        LinearLayout.LayoutParams.WRAP_CONTENT
                    ).apply { topMargin = resources.getDimensionPixelOffset(R.dimen.chip_spacing) }
                    setOnClickListener {
                        if (!host.isConnected()) {
                            Toast.makeText(requireContext(), getString(R.string.hint_connect_first), Toast.LENGTH_SHORT).show()
                            return@setOnClickListener
                        }
                        host.sendClusterText(preset)
                        Toast.makeText(requireContext(), getString(R.string.toast_sent_to_cluster), Toast.LENGTH_SHORT).show()
                    }
                    setOnLongClickListener {
                        val updated = loadPresets().filter { it != preset }
                        savePresets(updated)
                        refreshPresetButtons()
                        Toast.makeText(requireContext(), getString(R.string.cluster_preset_delete), Toast.LENGTH_SHORT).show()
                        true
                    }
                }
                presetList.addView(btn)
            }
        }

        view.findViewById<MaterialButton>(R.id.buttonAddPreset).setOnClickListener {
            val text = editCluster.text?.toString()?.trim() ?: ""
            if (text.isEmpty()) {
                Toast.makeText(requireContext(), getString(R.string.cluster_hint), Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            val current = loadPresets()
            if (current.size >= MAX_PRESETS) {
                Toast.makeText(requireContext(), getString(R.string.cluster_presets) + " (max $MAX_PRESETS)", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            if (text in current) {
                Toast.makeText(requireContext(), getString(R.string.cluster_preset_add), Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            savePresets(current + text)
            refreshPresetButtons()
            Toast.makeText(requireContext(), getString(R.string.cluster_preset_added), Toast.LENGTH_SHORT).show()
        }

        refreshPresetButtons()
    }
}
