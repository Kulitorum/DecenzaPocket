package io.github.kulitorum.decenzapocket;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;

import com.google.firebase.messaging.FirebaseMessagingService;
import com.google.firebase.messaging.RemoteMessage;

import org.json.JSONObject;

import java.util.Map;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.WebSocket;
import okhttp3.WebSocketListener;

public class DecenzaFcmService extends FirebaseMessagingService {

    private static final String TAG = "DecenzaFcm";
    private static final String QT_PREFS_NAME = "Decenza.DecenzaPocket";
    private static final String WS_URL = "wss://ws.decenza.coffee";

    @Override
    public void onMessageReceived(RemoteMessage message) {
        Map<String, String> data = message.getData();
        String isAwakeStr = data.get("isAwake");
        String deviceId = data.get("device_id");

        if (isAwakeStr != null && deviceId != null) {
            boolean isAwake = Boolean.parseBoolean(isAwakeStr);
            Log.i(TAG, "State update: device=" + deviceId + " isAwake=" + isAwake);

            PowerWidgetProvider.setIsAwake(this, isAwake);
            PowerWidgetProvider.refreshAllWidgets(this);
        }
    }

    @Override
    public void onNewToken(String token) {
        Log.i(TAG, "New FCM token");
        new Thread(() -> registerToken(this, token)).start();
    }

    static void registerToken(Context context, String token) {
        SharedPreferences qtPrefs = context.getSharedPreferences(QT_PREFS_NAME, Context.MODE_PRIVATE);
        String deviceId = qtPrefs.getString("device/id", "");
        String pairingToken = qtPrefs.getString("device/pairingToken", "");

        if (deviceId.isEmpty() || pairingToken.isEmpty()) {
            Log.w(TAG, "Not paired, skipping FCM registration");
            return;
        }

        OkHttpClient client = new OkHttpClient.Builder()
                .readTimeout(10, TimeUnit.SECONDS)
                .build();

        Request request = new Request.Builder().url(WS_URL).build();
        CountDownLatch latch = new CountDownLatch(1);

        WebSocket ws = client.newWebSocket(request, new WebSocketListener() {
            @Override
            public void onOpen(WebSocket webSocket, okhttp3.Response response) {
                try {
                    JSONObject register = new JSONObject();
                    register.put("action", "register");
                    register.put("device_id", deviceId);
                    register.put("role", "controller");
                    register.put("pairing_token", pairingToken);
                    webSocket.send(register.toString());
                } catch (Exception e) {
                    Log.e(TAG, "Failed to register", e);
                    latch.countDown();
                }
            }

            @Override
            public void onMessage(WebSocket webSocket, String text) {
                try {
                    JSONObject msg = new JSONObject(text);
                    String type = msg.optString("type");

                    if ("registered".equals(type)) {
                        JSONObject fcmReg = new JSONObject();
                        fcmReg.put("action", "register_fcm_token");
                        fcmReg.put("device_id", deviceId);
                        fcmReg.put("pairing_token", pairingToken);
                        fcmReg.put("fcm_token", token);
                        fcmReg.put("platform", "android");
                        webSocket.send(fcmReg.toString());
                    } else if ("fcm_token_registered".equals(type)) {
                        Log.i(TAG, "FCM token registered with relay");
                        latch.countDown();
                        webSocket.close(1000, "done");
                    }
                } catch (Exception e) {
                    Log.e(TAG, "Failed to handle message", e);
                    latch.countDown();
                }
            }

            @Override
            public void onFailure(WebSocket webSocket, Throwable t, okhttp3.Response response) {
                Log.e(TAG, "WebSocket failure during FCM registration", t);
                latch.countDown();
            }
        });

        try {
            latch.await(15, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            Log.w(TAG, "FCM registration timed out");
        }

        client.dispatcher().executorService().shutdown();
    }
}
