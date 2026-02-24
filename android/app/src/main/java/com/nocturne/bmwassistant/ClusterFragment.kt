package com.nocturne.bmwassistant

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.fragment.app.Fragment
import com.google.android.material.button.MaterialButton
import com.google.android.material.textfield.TextInputEditText

class ClusterFragment : Fragment() {
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
    }
}
