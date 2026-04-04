package io.github.kulitorum.decenzapocket;

import android.content.SharedPreferences;
import android.util.Log;

import com.google.firebase.messaging.FirebaseMessaging;

import org.qtproject.qt.android.bindings.QtApplication;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLEncoder;

import org.json.JSONObject;

public class DecenzaApplication extends QtApplication {

    private static final String TAG = "DecenzaApp";
    private static final String QT_PREFS_NAME = "Decenza.DecenzaPocket";

    @Override
    public void onCreate() {
        super.onCreate();

        SharedPreferences qtPrefs = getSharedPreferences(QT_PREFS_NAME, MODE_PRIVATE);
        String deviceId = qtPrefs.getString("device/id", "");
        String pairingToken = qtPrefs.getString("device/pairingToken", "");

        if (!deviceId.isEmpty() && !pairingToken.isEmpty()) {
            // Register FCM token with relay
            FirebaseMessaging.getInstance().getToken().addOnSuccessListener(token -> {
                new Thread(() -> DecenzaFcmService.registerToken(this, token)).start();
            });

            // Fetch initial device state for widget
            new Thread(() -> fetchInitialState(deviceId, pairingToken)).start();
        }
    }

    private void fetchInitialState(String deviceId, String pairingToken) {
        try {
            String query = "device_id=" + URLEncoder.encode(deviceId, "UTF-8")
                    + "&pairing_token=" + URLEncoder.encode(pairingToken, "UTF-8");
            URL url = new URL("https://api.decenza.coffee/v1/device-state?" + query);
            HttpURLConnection conn = (HttpURLConnection) url.openConnection();
            conn.setRequestMethod("GET");
            conn.setConnectTimeout(5000);
            conn.setReadTimeout(5000);

            if (conn.getResponseCode() == 200) {
                InputStream is = conn.getInputStream();
                ByteArrayOutputStream baos = new ByteArrayOutputStream();
                byte[] buf = new byte[1024];
                int len;
                while ((len = is.read(buf)) != -1) {
                    baos.write(buf, 0, len);
                }
                is.close();
                JSONObject json = new JSONObject(baos.toString("UTF-8"));
                boolean isAwake = json.optBoolean("isAwake", false);
                PowerWidgetProvider.setIsAwake(this, isAwake);
                PowerWidgetProvider.refreshAllWidgets(this);
                Log.i(TAG, "Initial device state: isAwake=" + isAwake);
            }
            conn.disconnect();
        } catch (Exception e) {
            Log.w(TAG, "Failed to fetch initial device state", e);
        }
    }
}
