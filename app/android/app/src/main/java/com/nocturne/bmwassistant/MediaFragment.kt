package com.nocturne.bmwassistant

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.fragment.app.Fragment
import com.google.android.material.button.MaterialButton
import com.google.android.material.textfield.TextInputEditText

class MediaFragment : Fragment() {
    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        return inflater.inflate(R.layout.fragment_media, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        val host = activity as? BleAssistantHost ?: return
        val editTrack = view.findViewById<TextInputEditText>(R.id.editTrack)
        val editArtist = view.findViewById<TextInputEditText>(R.id.editArtist)
        view.findViewById<MaterialButton>(R.id.buttonSendNowPlaying).setOnClickListener {
            if (!host.isConnected()) {
                Toast.makeText(requireContext(), getString(R.string.hint_connect_first), Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            val track = editTrack.text?.toString()?.trim() ?: ""
            val artist = editArtist.text?.toString()?.trim() ?: ""
            if (track.isEmpty() && artist.isEmpty()) {
                Toast.makeText(requireContext(), getString(R.string.track_hint), Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            host.sendNowPlaying(track, artist)
            Toast.makeText(requireContext(), getString(R.string.toast_sent), Toast.LENGTH_SHORT).show()
        }
    }
}
