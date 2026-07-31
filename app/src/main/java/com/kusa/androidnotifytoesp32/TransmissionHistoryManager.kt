package com.kusa.androidnotifytoesp32

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

data class HistoryEntry(
    val timestamp: String,
    val destination: String,
    val title: String,
    val isSuccess: Boolean,
    val message: String
)

object TransmissionHistoryManager {
    private val _history = MutableStateFlow<List<HistoryEntry>>(emptyList())
    val history: StateFlow<List<HistoryEntry>> = _history.asStateFlow()

    private const val MAX_HISTORY_SIZE = 20

    fun addEntry(destination: String, title: String, isSuccess: Boolean, message: String) {
        val sdf = SimpleDateFormat("HH:mm:ss", Locale.getDefault())
        val timestamp = sdf.format(Date())
        val newEntry = HistoryEntry(timestamp, destination, title, isSuccess, message)
        
        val currentList = _history.value.toMutableList()
        currentList.add(0, newEntry) // 最新を上に
        
        if (currentList.size > MAX_HISTORY_SIZE) {
            currentList.removeAt(currentList.size - 1)
        }
        
        _history.value = currentList
    }
}
