package com.nocturne.bmwassistant

interface BleAssistantHost {
    fun sendCommand(byte: Int)
    fun sendNowPlaying(track: String, artist: String)
    fun sendClusterText(text: String)
    fun isConnected(): Boolean
}
