// TamicApplication — класс приложения TAMIC (наследник QtApplication).
//
// Единственная добавка — проверочный запуск без касаний (решение A14): если окно
// открыто интентом с экстрой autotest или screen (am start … -e autotest all), оно
// показывается поверх экрана блокировки, включает экран и не даёт ему погаснуть.
// Делается при создании окна, до его показа: пока экран выключен, Qt не запускает
// main(), и сделать это из C++ нельзя. Обычный запуск (значок, уведомление) не затронут.
package ru.tamic.app;

import android.app.Activity;
import android.app.Application;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.os.Bundle;
import android.view.WindowManager;

import org.qtproject.qt.android.bindings.QtApplication;

public class TamicApplication extends QtApplication {
    @Override
    public void onCreate() {
        super.onCreate();
        registerActivityLifecycleCallbacks(new Application.ActivityLifecycleCallbacks() {
            @Override
            public void onActivityCreated(Activity a, Bundle state) {
                Intent it = a.getIntent();
                if (it == null || (!it.hasExtra("autotest") && !it.hasExtra("screen"))) return;
                a.setShowWhenLocked(true);
                a.setTurnScreenOn(true);
                a.getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                // -e orientation landscape|portrait — снимки в обоих положениях без поворота
                // телефона руками (в манифесте ориентация не фиксирована).
                String o = it.getStringExtra("orientation");
                if ("landscape".equals(o))
                    a.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
                else if ("portrait".equals(o))
                    a.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
            }
            @Override public void onActivityStarted(Activity a) {}
            @Override public void onActivityResumed(Activity a) {}
            @Override public void onActivityPaused(Activity a) {}
            @Override public void onActivityStopped(Activity a) {}
            @Override public void onActivitySaveInstanceState(Activity a, Bundle b) {}
            @Override public void onActivityDestroyed(Activity a) {}
        });
    }
}
