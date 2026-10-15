package com.nexo.nexo_app

import android.content.Context
import android.content.Intent
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import android.net.wifi.WifiNetworkSpecifier
import android.os.Build
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodChannel

class MainActivity : FlutterActivity() {

    private val backgroundChannel = "com.nexo.nexo_app/background"
    private val wifiChannel = "com.nexo.nexo_app/wifi"

    private var wifiCallback: ConnectivityManager.NetworkCallback? = null
    private var wifiNetwork: Network? = null

    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)

        MethodChannel(flutterEngine.dartExecutor.binaryMessenger, backgroundChannel)
            .setMethodCallHandler { call, result ->
                when (call.method) {
                    "start" -> {
                        val intent = Intent(this, BackgroundKeeperService::class.java)
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                            startForegroundService(intent)
                        } else {
                            startService(intent)
                        }
                        result.success(true)
                    }
                    "stop" -> {
                        stopService(Intent(this, BackgroundKeeperService::class.java))
                        result.success(true)
                    }
                    else -> result.notImplemented()
                }
            }

        MethodChannel(flutterEngine.dartExecutor.binaryMessenger, wifiChannel)
            .setMethodCallHandler { call, result ->
                when (call.method) {
                    "supported" -> result.success(Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q)
                    "join" -> {
                        val ssid = call.argument<String>("ssid")
                        val password = call.argument<String>("password") ?: ""
                        joinWifi(ssid, password, result)
                    }
                    "leave" -> {
                        leaveWifi()
                        result.success(true)
                    }
                    else -> result.notImplemented()
                }
            }
    }

    private fun joinWifi(ssid: String?, password: String, result: MethodChannel.Result) {
        if (ssid.isNullOrEmpty()) {
            result.error("bad_args", "Missing SSID", null)
            return
        }
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            result.error("unsupported", "Wi-Fi auto-join requires Android 10+", null)
            return
        }
        leaveWifi()

        val cm = getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
        val specifierBuilder = WifiNetworkSpecifier.Builder().setSsid(ssid)
        if (password.isNotEmpty()) {
            specifierBuilder.setWpa2Passphrase(password)
        }
        val request = NetworkRequest.Builder()
            .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
            .removeCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
            .setNetworkSpecifier(specifierBuilder.build())
            .build()

        var replied = false
        val callback = object : ConnectivityManager.NetworkCallback() {
            override fun onAvailable(network: Network) {
                wifiNetwork = network
                cm.bindProcessToNetwork(network)
                if (!replied) {
                    replied = true
                    result.success(true)
                }
            }

            override fun onUnavailable() {
                if (!replied) {
                    replied = true
                    result.error("unavailable", "Could not join $ssid", null)
                }
            }

            override fun onLost(network: Network) {
                if (wifiNetwork == network) {
                    cm.bindProcessToNetwork(null)
                    wifiNetwork = null
                }
            }
        }
        wifiCallback = callback
        cm.requestNetwork(request, callback)
    }

    private fun leaveWifi() {
        val cm = getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
        wifiCallback?.let {
            try {
                cm.unregisterNetworkCallback(it)
            } catch (_: Exception) {
            }
        }
        wifiCallback = null
        if (wifiNetwork != null) {
            cm.bindProcessToNetwork(null)
            wifiNetwork = null
        }
    }

    override fun onDestroy() {
        leaveWifi()
        super.onDestroy()
    }
}
