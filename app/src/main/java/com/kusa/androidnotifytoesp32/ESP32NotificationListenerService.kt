package com.kusa.androidnotifytoesp32

import android.Manifest
import android.app.Notification
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothSocket
import android.content.Context
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Typeface
import android.service.notification.NotificationListenerService
import android.service.notification.StatusBarNotification
import android.util.Log
import androidx.core.content.ContextCompat
import java.io.DataOutputStream
import java.net.Socket
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.UUID
import kotlin.concurrent.thread

class ESP32NotificationListenerService : NotificationListenerService() {

    private val TAG = "ESP32Notify"
    private val WIDTH = 320
    private val HEIGHT = 170
    private val SPP_UUID: UUID = UUID.fromString("00001101-0000-1000-8000-00805f9b34fb")

    override fun onNotificationPosted(sbn: StatusBarNotification) {
        val packageName = sbn.packageName
        if (packageName == this.packageName) return // 自アプリの通知は無視

        val extras = sbn.notification.extras
        val title = extras.getString(Notification.EXTRA_TITLE) ?: ""
        val text = extras.getCharSequence(Notification.EXTRA_TEXT)?.toString() ?: ""

        Log.d(TAG, "Notification received: [$title] $text")

        val bitmap = createNotificationBitmap(title, text)
        val rgb565Data = convertToRGB565(bitmap)
        
        val prefs = getSharedPreferences("Settings", Context.MODE_PRIVATE)
        val duration = prefs.getInt("display_duration", 10).coerceIn(0, 300)
        val header = byteArrayOf(
            'N'.code.toByte(),
            'T'.code.toByte(),
            (duration shr 8).toByte(),
            (duration and 0xFF).toByte()
        )
        val payload = header + rgb565Data

        val mode = prefs.getString("mode", "TCP")
        
        if (mode == "BT") {
            sendToESP32Bluetooth(payload)
        } else {
            val ip = prefs.getString("esp32_ip", "esp32-notify.local") ?: "esp32-notify.local"
            sendToESP32Tcp(payload, ip)
        }
    }

    private fun createNotificationBitmap(title: String, body: String): Bitmap {
        val bitmap = Bitmap.createBitmap(WIDTH, HEIGHT, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(bitmap)
        canvas.drawColor(Color.BLACK)

        val paint = Paint().apply {
            isAntiAlias = true
            typeface = Typeface.DEFAULT
        }

        // タイトル (青色)
        paint.color = Color.rgb(0, 120, 255)
        paint.textSize = 18f
        canvas.drawText("📱 $title", 12f, 30f, paint)

        // 本文 (白色)
        paint.color = Color.WHITE
        paint.textSize = 16f
        
        // 簡易的な改行処理
        val maxWidth = WIDTH - 24
        val lines = mutableListOf<String>()
        var remainingText = body
        while (remainingText.isNotEmpty() && lines.size < 4) {
            val count = paint.breakText(remainingText, true, maxWidth.toFloat(), null)
            lines.add(remainingText.substring(0, count))
            remainingText = remainingText.substring(count)
        }

        lines.forEachIndexed { index, line ->
            canvas.drawText(line, 12f, 60f + index * 24f, paint)
        }

        // タイムスタンプ (青色)
        paint.color = Color.rgb(0, 120, 255)
        paint.textSize = 18f
        val timeStr = SimpleDateFormat("HH:mm", Locale.getDefault()).format(Date())
        val timeWidth = paint.measureText(timeStr)
        canvas.drawText(timeStr, WIDTH - timeWidth - 12f, HEIGHT - 15f, paint)

        return bitmap
    }

    private fun convertToRGB565(bitmap: Bitmap): ByteArray {
        val bytes = ByteArray(WIDTH * HEIGHT * 2)
        var index = 0
        for (y in 0 until HEIGHT) {
            for (x in 0 until WIDTH) {
                val pixel = bitmap.getPixel(x, y)
                val r = Color.red(pixel)
                val g = Color.green(pixel)
                val b = Color.blue(pixel)

                // R5 G6 B5
                val r5 = (r shr 3) and 0x1F
                val g6 = (g shr 2) and 0x3F
                val b5 = (b shr 3) and 0x1F

                val rgb565 = (r5 shl 11) or (g6 shl 5) or b5
                
                // Little Endian (<H)
                bytes[index++] = (rgb565 and 0xFF).toByte()
                bytes[index++] = (rgb565 shr 8).toByte()
            }
        }
        return bytes
    }

    private fun resolveHost(host: String): String {
        return try {
            val address = java.net.InetAddress.getByName(host)
            address.hostAddress ?: host
        } catch (e: Exception) {
            Log.w(TAG, "DNS resolution failed for $host: ${e.message}")
            host
        }
    }

    private fun sendToESP32Tcp(data: ByteArray, host: String) {
        thread {
            try {
                val resolvedTarget = resolveHost(host)
                Socket(resolvedTarget, 5555).use { socket ->
                    socket.soTimeout = 5000
                    val out = DataOutputStream(socket.getOutputStream())
                    out.write(data)
                    out.flush()
                    Log.d(TAG, "Successfully sent ${data.size} bytes to ESP32 via TCP ($host -> $resolvedTarget)")
                    TransmissionHistoryManager.addEntry("TCP: $host", "画像送信", true, "成功")
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error sending to ESP32 via TCP ($host): ${e.message}")
                TransmissionHistoryManager.addEntry("TCP: $host", "画像送信", false, "エラー: ${e.message}")
            }
        }
    }

    private fun sendToESP32Bluetooth(data: ByteArray) {
        thread {
            val prefs = getSharedPreferences("Settings", Context.MODE_PRIVATE)
            val targetName = prefs.getString("bt_name", "ESP32_Notfity") ?: "ESP32_Notfity"
            try {
                val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
                val bluetoothAdapter = bluetoothManager.adapter
                if (bluetoothAdapter == null || !bluetoothAdapter.isEnabled) {
                    val msg = "Bluetoothが無効です"
                    Log.e(TAG, msg)
                    TransmissionHistoryManager.addEntry("BT: $targetName", "画像送信", false, msg)
                    return@thread
                }

                if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
                    val msg = "権限がありません"
                    Log.e(TAG, msg)
                    TransmissionHistoryManager.addEntry("BT: $targetName", "画像送信", false, msg)
                    return@thread
                }

                val pairedDevices = bluetoothAdapter.bondedDevices
                val device = pairedDevices.find { it.name == targetName }
                
                if (device == null) {
                    val msg = "ペアリングされていません"
                    Log.e(TAG, msg)
                    TransmissionHistoryManager.addEntry("BT: $targetName", "画像送信", false, msg)
                    return@thread
                }

                device.createRfcommSocketToServiceRecord(SPP_UUID).use { socket ->
                    socket.connect()
                    val out = socket.outputStream
                    out.write(data)
                    out.flush()
                    Log.d(TAG, "Successfully sent ${data.size} bytes to ESP32 via Bluetooth ($targetName)")
                    TransmissionHistoryManager.addEntry("BT: $targetName", "画像送信", true, "成功")
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error sending to ESP32 via Bluetooth: ${e.message}")
                TransmissionHistoryManager.addEntry("BT: $targetName", "画像送信", false, "エラー: ${e.message}")
            }
        }
    }
}
