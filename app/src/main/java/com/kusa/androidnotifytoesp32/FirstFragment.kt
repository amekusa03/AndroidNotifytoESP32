package com.kusa.androidnotifytoesp32

import android.os.Bundle
import androidx.fragment.app.Fragment
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.navigation.fragment.findNavController
import android.Manifest
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.provider.Settings
import android.text.TextUtils
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.content.ContextCompat
import androidx.core.widget.addTextChangedListener
import androidx.lifecycle.lifecycleScope
import com.kusa.androidnotifytoesp32.databinding.FragmentFirstBinding
import kotlinx.coroutines.launch

/**
 * A simple [Fragment] subclass as the default destination in the navigation.
 */
class FirstFragment : Fragment() {

    private var _binding: FragmentFirstBinding? = null
    private val binding get() = _binding!!

    private val requestPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { permissions ->
        updateStatus()
    }

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentFirstBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val prefs = requireContext().getSharedPreferences("Settings", Context.MODE_PRIVATE)
        
        binding.editTextIp.setText(prefs.getString("esp32_ip", "esp32-notify.local"))
        binding.editTextBtName.setText(prefs.getString("bt_name", "ESP32_Notfity"))
        binding.editTextDuration.setText(prefs.getInt("display_duration", 10).toString())
        
        val mode = prefs.getString("mode", "TCP")
        if (mode == "BT") {
            binding.radioButtonBt.isChecked = true
        } else {
            binding.radioButtonTcp.isChecked = true
        }

        binding.editTextIp.addTextChangedListener {
            prefs.edit().putString("esp32_ip", it.toString()).apply()
        }
        binding.editTextBtName.addTextChangedListener {
            prefs.edit().putString("bt_name", it.toString()).apply()
        }
        binding.editTextDuration.addTextChangedListener { text ->
            val duration = text.toString().toIntOrNull()?.coerceIn(0, 300) ?: 10
            prefs.edit().putInt("display_duration", duration).apply()
        }
        binding.radioGroupMode.setOnCheckedChangeListener { _, checkedId ->
            val newMode = if (checkedId == R.id.radioButton_bt) "BT" else "TCP"
            prefs.edit().putString("mode", newMode).apply()
        }

        binding.buttonSettings.setOnClickListener {
            val intent = Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS)
            startActivity(intent)
        }

        checkBluetoothPermissions()
        observeHistory()
    }

    private fun observeHistory() {
        viewLifecycleOwner.lifecycleScope.launch {
            TransmissionHistoryManager.history.collect { entries ->
                if (entries.isEmpty()) {
                    binding.textViewHistory.text = "履歴はありません"
                    return@collect
                }
                val sb = StringBuilder()
                entries.forEach { entry ->
                    val status = if (entry.isSuccess) "✅" else "❌"
                    sb.append("[$status ${entry.timestamp}] ${entry.destination}\n")
                    sb.append("  ${entry.message}\n\n")
                }
                binding.textViewHistory.text = sb.toString()
            }
        }
    }

    private fun checkBluetoothPermissions() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            val permissions = arrayOf(
                Manifest.permission.BLUETOOTH_SCAN,
                Manifest.permission.BLUETOOTH_CONNECT
            )
            val toRequest = permissions.filter {
                ContextCompat.checkSelfPermission(requireContext(), it) != PackageManager.PERMISSION_GRANTED
            }
            if (toRequest.isNotEmpty()) {
                requestPermissionLauncher.launch(toRequest.toTypedArray())
            }
        }
    }

    override fun onResume() {
        super.onResume()
        updateStatus()
    }

    private fun updateStatus() {
        val isNotifyEnabled = isNotificationServiceEnabled()
        val isBtGranted = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            ContextCompat.checkSelfPermission(requireContext(), Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED
        } else true

        if (isNotifyEnabled && isBtGranted) {
            binding.textviewStatus.text = "通知転送ステータス: 稼働中"
            binding.textviewStatus.setTextColor(ContextCompat.getColor(requireContext(), android.R.color.holo_green_dark))
            binding.buttonSettings.text = "設定を確認する"
        } else {
            var errorMsg = "通知転送ステータス: "
            if (!isNotifyEnabled) errorMsg += "通知権限なし "
            if (!isBtGranted) errorMsg += "BT権限なし"
            binding.textviewStatus.text = errorMsg
            binding.textviewStatus.setTextColor(ContextCompat.getColor(requireContext(), android.R.color.holo_red_dark))
            binding.buttonSettings.text = "権限を設定する"
        }
    }

    private fun isNotificationServiceEnabled(): Boolean {
        val pkgName = requireContext().packageName
        val flat = Settings.Secure.getString(requireContext().contentResolver, "enabled_notification_listeners")
        if (!TextUtils.isEmpty(flat)) {
            val names = flat.split(":")
            for (name in names) {
                val cn = ComponentName.unflattenFromString(name)
                if (cn != null && TextUtils.equals(pkgName, cn.packageName)) {
                    return true
                }
            }
        }
        return false
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}