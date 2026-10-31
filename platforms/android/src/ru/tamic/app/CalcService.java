// CalcService — служба переднего плана на время расчёта (этап П3).
//
// Пока идёт расчёт, система не выгружает приложение и его дочерний процесс ядра:
//  * уведомление с ходом расчёта и кнопкой «Остановить»;
//  * PARTIAL_WAKE_LOCK — процессор не засыпает при выключенном экране;
//  * тип службы specialUse (Android 14+): у dataSync с Android 15 предел 6 ч в
//    сутки, а расчёт бывает дольше (решение A12, 🟡).
// Ядро работает отдельным процессом (QProcess, решение A3); служба только держит
// приложение «на переднем плане».
package ru.tamic.app;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.graphics.drawable.Icon;
import android.os.Build;
import android.os.IBinder;
import android.os.PowerManager;
import android.util.Log;

public class CalcService extends Service {
    static final String ACTION_START = "ru.tamic.app.CALC_START";
    static final String ACTION_STOP = "ru.tamic.app.CALC_STOP";
    static final String EXTRA_TITLE = "title";
    private static final String CHANNEL = "calc";
    private static final int NOTIF_ID = 1001;
    private static final long WAKE_LOCK_MAX_MS = 48L * 3600L * 1000L;

    private static volatile boolean sRunning = false;
    private PowerManager.WakeLock mWakeLock;

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        String action = intent != null ? intent.getAction() : null;
        if (ACTION_STOP.equals(action)) {
            try {
                TamicBridge.nativeStopRequested();
            } catch (UnsatisfiedLinkError e) {
                Log.w("TAMIC", "nativeStopRequested: " + e);
            }
            return START_NOT_STICKY;
        }
        String title = intent != null ? intent.getStringExtra(EXTRA_TITLE) : null;
        if (title == null) title = "TMC Suite";
        ensureChannel(this);
        Notification n = build(this, title, TamicBridge.tr("Подготовка…", "Preparing…"), -1);
        if (Build.VERSION.SDK_INT >= 34)
            startForeground(NOTIF_ID, n, ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE);
        else
            startForeground(NOTIF_ID, n);
        if (mWakeLock == null) {
            PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
            mWakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "TAMIC:calc");
            mWakeLock.setReferenceCounted(false);
        }
        mWakeLock.acquire(WAKE_LOCK_MAX_MS);
        sRunning = true;
        return START_NOT_STICKY;
    }

    @Override
    public void onDestroy() {
        sRunning = false;
        if (mWakeLock != null && mWakeLock.isHeld()) mWakeLock.release();
        stopForeground(STOP_FOREGROUND_REMOVE);
        super.onDestroy();
    }

    /** Обновить уведомление (ход в процентах; < 0 — без шкалы). */
    static void update(Context ctx, String title, String text, int percent) {
        if (!sRunning) return;
        NotificationManager nm = ctx.getSystemService(NotificationManager.class);
        if (nm != null) nm.notify(NOTIF_ID, build(ctx, title, text, percent));
    }

    private static void ensureChannel(Context ctx) {
        NotificationManager nm = ctx.getSystemService(NotificationManager.class);
        if (nm == null) return;
        // Создаётся заново при каждом расчёте: так имя канала следует языку приложения.
        NotificationChannel ch = new NotificationChannel(CHANNEL, TamicBridge.tr("Расчёт", "Calculation"),
                NotificationManager.IMPORTANCE_LOW);
        ch.setDescription(TamicBridge.tr("Ход расчёта и кнопка «Остановить»", "Calculation progress and the Stop button"));
        ch.setShowBadge(false);
        nm.createNotificationChannel(ch);
    }

    private static Notification build(Context ctx, String title, String text, int percent) {
        Intent open = ctx.getPackageManager().getLaunchIntentForPackage(ctx.getPackageName());
        PendingIntent contentPi = null;
        if (open != null) {
            open.setFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_REORDER_TO_FRONT);
            contentPi = PendingIntent.getActivity(ctx, 0, open,
                    PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
        }
        Intent stop = new Intent(ctx, CalcService.class).setAction(ACTION_STOP);
        PendingIntent stopPi = PendingIntent.getService(ctx, 1, stop,
                PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
        Notification.Builder b = new Notification.Builder(ctx, CHANNEL)
                .setSmallIcon(R.drawable.ic_stat_tamic)
                .setContentTitle(title)
                .setContentText(text)
                .setOngoing(true)
                .setOnlyAlertOnce(true)
                .setShowWhen(false)
                .setCategory(Notification.CATEGORY_PROGRESS)
                .setProgress(100, Math.max(0, Math.min(100, percent)), percent < 0)
                .addAction(new Notification.Action.Builder(
                        Icon.createWithResource(ctx, R.drawable.ic_stat_tamic),
                        TamicBridge.tr("Остановить", "Stop"), stopPi).build());
        if (contentPi != null) b.setContentIntent(contentPi);
        if (Build.VERSION.SDK_INT >= 31)
            b.setForegroundServiceBehavior(Notification.FOREGROUND_SERVICE_IMMEDIATE);
        return b.build();
    }
}
