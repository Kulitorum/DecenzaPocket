package io.github.kulitorum.decenzapocket;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.core.app.JobIntentService;

import org.json.JSONObject;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.Response;
import okhttp3.WebSocket;
import okhttp3.WebSocketListener;

public class PowerWidgetCommandService extends JobIntentService {

    private static final String TAG = "PowerWidgetCmd";
    private static final int JOB_ID = 1001;
    private static final String QT_PREFS_NAME = "Decenza.DecenzaPocket";
    private static final String WS_URL = "wss://ws.decenza.coffee";

    public static void enqueueWork(Context context, Intent intent) {
        enqueueWork(context, PowerWidgetCommandService.class, JOB_ID, intent);
    }

    @Override
    protected void onHandleWork(@NonNull Intent intent) {
        try {
            SharedPreferences qtPrefs = getSharedPreferences(QT_PREFS_NAME, Context.MODE_PRIVATE);
            String deviceId = qtPrefs.getString("device/id", "");
            String pairingToken = qtPrefs.getString("device/pairingToken", "");

            if (deviceId.isEmpty() || pairingToken.isEmpty()) {
                Log.w(TAG, "Not paired, launching app");
                launchApp();
                return;
            }

            boolean currentlyAwake = PowerWidgetProvider.getIsAwake(this);
            String command = currentlyAwake ? "sleep" : "wake";

            boolean success = sendCommand(deviceId, pairingToken, command);

            if (success) {
                boolean newState = !currentlyAwake;
                PowerWidgetProvider.setIsAwake(this, newState);
                PowerWidgetProvider.refreshAllWidgets(this);
                Log.i(TAG, "Command " + command + " sent successfully");
            } else {
                Log.w(TAG, "Command failed, launching app");
                launchApp();
            }

        } catch (Exception e) {
            Log.e(TAG, "Error in widget command", e);
            launchApp();
        }
    }

    private boolean sendCommand(String deviceId, String pairingToken, String command) {
        OkHttpClient client = new OkHttpClient.Builder()
                .readTimeout(10, TimeUnit.SECONDS)
                .build();

        Request request = new Request.Builder().url(WS_URL).build();

        CountDownLatch commandLatch = new CountDownLatch(1);
        final boolean[] success = {false};

        WebSocket ws = client.newWebSocket(request, new WebSocketListener() {
            @Override
            public void onOpen(WebSocket webSocket, Response response) {
                try {
                    JSONObject register = new JSONObject();
                    register.put("action", "register");
                    register.put("device_id", deviceId);
                    register.put("role", "controller");
                    register.put("pairing_token", pairingToken);
                    webSocket.send(register.toString());
                } catch (Exception e) {
                    Log.e(TAG, "Failed to send register", e);
                    commandLatch.countDown();
                }
            }

            @Override
            public void onMessage(WebSocket webSocket, String text) {
                try {
                    JSONObject msg = new JSONObject(text);
                    String type = msg.optString("type");

                    if ("registered".equals(type)) {
                        JSONObject cmd = new JSONObject();
                        cmd.put("action", "command");
                        cmd.put("device_id", deviceId);
                        cmd.put("pairing_token", pairingToken);
                        cmd.put("command", command);
                        webSocket.send(cmd.toString());
                    } else if ("command_sent".equals(type)) {
                        success[0] = true;
                        commandLatch.countDown();
                        webSocket.close(1000, "done");
                    } else if ("error".equals(type)) {
                        Log.w(TAG, "Relay error: " + msg.optString("error"));
                        commandLatch.countDown();
                        webSocket.close(1000, "error");
                    }
                } catch (Exception e) {
                    Log.e(TAG, "Failed to handle message", e);
                    commandLatch.countDown();
                }
            }

            @Override
            public void onFailure(WebSocket webSocket, Throwable t, Response response) {
                Log.e(TAG, "WebSocket failure", t);
                commandLatch.countDown();
            }
        });

        try {
            commandLatch.await(15, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            Log.w(TAG, "Command timed out");
        }

        client.dispatcher().executorService().shutdown();
        return success[0];
    }

    private void launchApp() {
        Intent launchIntent = getPackageManager().getLaunchIntentForPackage(getPackageName());
        if (launchIntent != null) {
            launchIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            startActivity(launchIntent);
        }
    }
}
