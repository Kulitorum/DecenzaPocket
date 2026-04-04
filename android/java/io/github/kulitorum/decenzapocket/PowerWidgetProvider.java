package io.github.kulitorum.decenzapocket;

import android.app.PendingIntent;
import android.appwidget.AppWidgetManager;
import android.appwidget.AppWidgetProvider;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.widget.RemoteViews;

public class PowerWidgetProvider extends AppWidgetProvider {

    public static final String ACTION_TOGGLE = "io.github.kulitorum.decenzapocket.TOGGLE_POWER";
    public static final String ACTION_STATE_CHANGED = "io.github.kulitorum.decenzapocket.STATE_CHANGED";
    private static final String PREFS_NAME = "decenza_widget";
    private static final String KEY_IS_AWAKE = "is_awake";

    @Override
    public void onUpdate(Context context, AppWidgetManager manager, int[] appWidgetIds) {
        for (int id : appWidgetIds) {
            updateWidget(context, manager, id);
        }
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        super.onReceive(context, intent);

        if (ACTION_TOGGLE.equals(intent.getAction())) {
            Intent serviceIntent = new Intent(context, PowerWidgetCommandService.class);
            PowerWidgetCommandService.enqueueWork(context, serviceIntent);
        } else if (ACTION_STATE_CHANGED.equals(intent.getAction())) {
            refreshAllWidgets(context);
        }
    }

    static void updateWidget(Context context, AppWidgetManager manager, int widgetId) {
        boolean isAwake = getIsAwake(context);

        RemoteViews views = new RemoteViews(context.getPackageName(), R.layout.power_widget);
        views.setImageViewResource(R.id.widget_icon,
                isAwake ? R.drawable.widget_on : R.drawable.widget_off);

        Intent toggleIntent = new Intent(context, PowerWidgetProvider.class);
        toggleIntent.setAction(ACTION_TOGGLE);
        PendingIntent pendingIntent = PendingIntent.getBroadcast(
                context, 0, toggleIntent, PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        views.setOnClickPendingIntent(R.id.widget_icon, pendingIntent);

        manager.updateAppWidget(widgetId, views);
    }

    static void refreshAllWidgets(Context context) {
        AppWidgetManager manager = AppWidgetManager.getInstance(context);
        int[] ids = manager.getAppWidgetIds(new ComponentName(context, PowerWidgetProvider.class));
        for (int id : ids) {
            updateWidget(context, manager, id);
        }
    }

    static boolean getIsAwake(Context context) {
        SharedPreferences prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        return prefs.getBoolean(KEY_IS_AWAKE, false);
    }

    static void setIsAwake(Context context, boolean isAwake) {
        context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
                .edit()
                .putBoolean(KEY_IS_AWAKE, isAwake)
                .apply();
    }
}
